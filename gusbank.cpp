#include "gusbank.h"

#include <cmath>
#include <vector>

#include "wadprocessor.h"

//
// This is libtimidity's instrument loader (instrum.c, resample.c pre_resample)
// run offline for GUSBANK_RATE, so that the GBA player doesn't need any
// floating point, file IO or allocation for instruments.
//

#define FRACTION_BITS GUSBANK_FRACTION_BITS
#define FRACTION_MASK (~(0xFFFFFFFF << FRACTION_BITS))

#define MODES_16BIT     (1<<0)
#define MODES_UNSIGNED  (1<<1)
#define MODES_LOOPING   (1<<2)
#define MODES_PINGPONG  (1<<3)
#define MODES_REVERSE   (1<<4)
#define MODES_SUSTAIN   (1<<5)
#define MODES_ENVELOPE  (1<<6)

#define SWEEP_TUNING 38
#define VIBRATO_RATE_TUNING 38
#define TREMOLO_RATE_TUNING 38
#define SWEEP_SHIFT 16
#define RATE_SHIFT 5
#define SINE_CYCLE_LENGTH 1024
#define VIBRATO_SAMPLE_INCREMENTS 32

static const qint32 freq_table[128] =
{
    8176, 8662, 9177, 9723, 10301, 10913, 11562, 12250, 12978, 13750, 14568, 15434,
    16352, 17324, 18354, 19445, 20602, 21827, 23125, 24500, 25957, 27500, 29135, 30868,
    32703, 34648, 36708, 38891, 41203, 43654, 46249, 48999, 51913, 55000, 58270, 61735,
    65406, 69296, 73416, 77782, 82407, 87307, 92499, 97999, 103826, 110000, 116541, 123471,
    130813, 138591, 146832, 155563, 164814, 174614, 184997, 195998, 207652, 220000, 233082, 246942,
    261626, 277183, 293665, 311127, 329628, 349228, 369994, 391995, 415305, 440000, 466164, 493883,
    523251, 554365, 587330, 622254, 659255, 698456, 739989, 783991, 830609, 880000, 932328, 987767,
    1046502, 1108731, 1174659, 1244508, 1318510, 1396913, 1479978, 1567982, 1661219, 1760000, 1864655, 1975533,
    2093005, 2217461, 2349318, 2489016, 2637020, 2793826, 2959955, 3135963, 3322438, 3520000, 3729310, 3951066,
    4186009, 4434922, 4698636, 4978032, 5274041, 5587652, 5919911, 6271927, 6644875, 7040000, 7458620, 7902133,
    8372018, 8869844, 9397273, 9956063, 10548082, 11175303, 11839822, 12543854
};

class ToneEntry
{
public:
    QString name;
    int note = -1, amp = -1, pan = -1, strip_loop = -1, strip_envelope = -1, strip_tail = -1;
};

class GusSample
{
public:
    gusbank_sample_t s;
    qint32 sample_rate = 0;
    double volume = 1.0;
    std::vector<qint16> data;
};

class GusInstrument
{
public:
    QVector<GusSample> samples;
};

//Port of libtimidity's convert_* functions with song->rate = GUSBANK_RATE. FAST_DECAY is defined.
static qint32 ConvertEnvelopeRate(quint8 rate)
{
    qint32 r = 3 - ((rate >> 6) & 0x3);
    r *= 3;
    r = qint32(rate & 0x3f) << r;

    r = ((r * 44100) / GUSBANK_RATE) * GUSBANK_CONTROL_RATIO;

    return qint32(quint32(r) << 10);
}

static qint32 ConvertEnvelopeOffset(quint8 offset)
{
    return qint32(offset) << (7+15);
}

static qint32 ConvertTremoloSweep(quint8 sweep)
{
    if(!sweep)
        return 0;

    return ((GUSBANK_CONTROL_RATIO * SWEEP_TUNING) << SWEEP_SHIFT) / (GUSBANK_RATE * sweep);
}

static qint32 ConvertVibratoSweep(quint8 sweep, qint32 vib_control_ratio)
{
    if(!sweep)
        return 0;

    return qint32((double(vib_control_ratio) * SWEEP_TUNING * double(1 << SWEEP_SHIFT)) / double(GUSBANK_RATE * sweep));
}

static qint32 ConvertTremoloRate(quint8 rate)
{
    return ((SINE_CYCLE_LENGTH * GUSBANK_CONTROL_RATIO * rate) << RATE_SHIFT) / (TREMOLO_RATE_TUNING * GUSBANK_RATE);
}

static qint32 ConvertVibratoRate(quint8 rate)
{
    return (VIBRATO_RATE_TUNING * GUSBANK_RATE) / (rate * 2 * VIBRATO_SAMPLE_INCREMENTS);
}

//Port of libtimidity's pre_resample(). Resamples a fixed pitch sample to the output rate.
static void PreResample(GusSample& sp)
{
    gusbank_sample_t& s = sp.s;

    double a = (double(s.root_freq) * GUSBANK_RATE) / (double(sp.sample_rate) * freq_table[s.note_to_use]);

    if(s.data_length * a >= 0x7fffffffL)
        return;

    qint32 newlen = qint32(s.data_length * a);
    qint32 count = (newlen >> FRACTION_BITS) - 1;

    if(count < 2)
        return;

    qint32 incr = (s.data_length - (1 << FRACTION_BITS)) / count;
    qint32 ofs = incr;

    if(double(newlen) + incr >= 0x7fffffffL)
        return;

    const std::vector<qint16>& src = sp.data;
    std::vector<qint16> newdata((newlen >> FRACTION_BITS) + 4, 0);

    size_t dest = 0;

    if(--count)
        newdata[dest++] = src[0];

    auto at = [&src](qint64 i) -> qint32 { return (i >= 0 && i < qint64(src.size())) ? src[i] : 0; };

    count--;

    for(qint32 i = 0; i < count; i++)
    {
        qint64 vptr = ofs >> FRACTION_BITS;
        qint32 v1 = (vptr >= 1) ? at(vptr - 1) : 0;
        qint32 v2 = at(vptr);
        qint32 v3 = at(vptr + 1);
        qint32 v4 = at(vptr + 2);
        qint32 v5 = v2 - v3;
        double xdiff = double(ofs & FRACTION_MASK) / double(1 << FRACTION_BITS);

        qint32 v = qint32(v2 + xdiff * (1.0/6.0) * (3 * (v3 - v5) - 2 * v1 - v4 +
                    xdiff * (3 * (v1 - v2 - v5) + xdiff * (3 * v5 + v4 - v1))));

        newdata[dest++] = qint16((v > 32767) ? 32767 : ((v < -32768) ? -32768 : v));
        ofs += incr;
    }

    if(ofs & FRACTION_MASK)
    {
        qint32 v1 = at(ofs >> FRACTION_BITS);
        qint32 v2 = at((ofs >> FRACTION_BITS) + 1);
        newdata[dest++] = qint16(v1 + (((v2 - v1) * qint32(ofs & FRACTION_MASK)) >> FRACTION_BITS));
    }
    else
        newdata[dest++] = qint16(at(ofs >> FRACTION_BITS));

    newdata[dest] = newdata[dest - 1] / 2;
    dest++;
    newdata[dest] = newdata[dest - 1] / 2;

    s.data_length = newlen;
    s.loop_start = qint32(s.loop_start * a);
    s.loop_end = qint32(s.loop_end * a);

    sp.data = newdata;
    sp.sample_rate = 0;
}

static quint32 ReadLE32(const quint8* p) { return qFromLittleEndian<quint32>(p); }
static quint16 ReadLE16(const quint8* p) { return qFromLittleEndian<quint16>(p); }

//Port of libtimidity's load_instrument().
static bool LoadInstrument(const QString& path, GusInstrument& ip, int panning, int amp, int note_to_use,
                           int strip_loop, int strip_envelope, int strip_tail)
{
    QFile f(path);

    if(!f.open(QIODevice::ReadOnly))
    {
        qWarning("Instrument %s can't be found.", qPrintable(path));
        return false;
    }

    QByteArray file = f.readAll();
    f.close();

    const quint8* p = reinterpret_cast<const quint8*>(file.constData());
    const qint64 size = file.size();

    if(size < 239 || (memcmp(p, "GF1PATCH110\0ID#000002", 22) && memcmp(p, "GF1PATCH100\0ID#000002", 22)))
    {
        qWarning("%s: not an instrument", qPrintable(path));
        return false;
    }

    if(p[82] != 1 && p[82] != 0)
        return false;

    if(p[151] != 1 && p[151] != 0)
        return false;

    const int numSamples = p[198];

    qint64 pos = 239;

    for(int i = 0; i < numSamples; i++)
    {
        if(pos + 96 > size)
            return false;

        const quint8* h = p + pos;

        GusSample sp;
        gusbank_sample_t& s = sp.s;
        memset(&s, 0, sizeof(s));

        quint8 fractions = h[7];
        s.data_length = qint32(ReadLE32(h + 8));
        s.loop_start = qint32(ReadLE32(h + 12));
        s.loop_end = qint32(ReadLE32(h + 16));
        sp.sample_rate = ReadLE16(h + 20);
        s.low_freq = qint32(ReadLE32(h + 22));
        s.high_freq = qint32(ReadLE32(h + 26));
        s.root_freq = qint32(ReadLE32(h + 30));

        const quint8* tmp = h + 37; //envelope, tremolo and vibrato (18 bytes)

        (void)panning; //Mono output: panning isn't used.

        if(!tmp[13] || !tmp[14])
        {
            s.tremolo_sweep_increment = s.tremolo_phase_increment = s.tremolo_depth = 0;
        }
        else
        {
            s.tremolo_sweep_increment = ConvertTremoloSweep(tmp[12]);
            s.tremolo_phase_increment = ConvertTremoloRate(tmp[13]);
            s.tremolo_depth = tmp[14];
        }

        if(!tmp[16] || !tmp[17])
        {
            s.vibrato_sweep_increment = s.vibrato_control_ratio = s.vibrato_depth = 0;
        }
        else
        {
            s.vibrato_control_ratio = ConvertVibratoRate(tmp[16]);
            s.vibrato_sweep_increment = ConvertVibratoSweep(tmp[15], s.vibrato_control_ratio);
            s.vibrato_depth = tmp[17];
        }

        s.modes = h[55];

        pos += 96;

        if(note_to_use != -1)
            s.note_to_use = quint8(note_to_use);
        else
            s.note_to_use = 0;

        if(s.modes & MODES_LOOPING)
            s.modes |= MODES_SUSTAIN;

        if((strip_loop == 1) && (s.modes & (MODES_SUSTAIN | MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE)))
            s.modes &= ~(MODES_SUSTAIN | MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE);

        if(strip_envelope == 1)
        {
            s.modes &= ~MODES_ENVELOPE;
        }
        else if(strip_envelope != 0)
        {
            if(!(s.modes & (MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE)))
                s.modes &= ~(MODES_SUSTAIN|MODES_ENVELOPE);
            else if(!memcmp(tmp, "??????", 6) || tmp[11] >= 100)
                s.modes &= ~MODES_ENVELOPE;
            else if(!(s.modes & MODES_SUSTAIN))
                s.modes &= ~MODES_ENVELOPE;
        }

        for(int j = 0; j < 6; j++)
        {
            s.envelope_rate[j] = ConvertEnvelopeRate(tmp[j]);
            s.envelope_offset[j] = ConvertEnvelopeOffset(tmp[6+j]);
        }

        //Sample data, converted to signed 16 bit. Lengths become sample counts.
        const qint64 dataBytes = s.data_length;

        if(pos + dataBytes > size)
            return false;

        const quint8* d = p + pos;
        pos += dataBytes;

        if(s.modes & MODES_16BIT)
        {
            qint32 n = s.data_length / 2;
            sp.data.resize(n + 2, 0);

            for(qint32 k = 0; k < n; k++)
                sp.data[k] = qint16(ReadLE16(d + k * 2));

            s.data_length /= 2;
            s.loop_start /= 2;
            s.loop_end /= 2;
        }
        else
        {
            qint32 n = s.data_length;
            sp.data.resize(n + 2, 0);

            for(qint32 k = 0; k < n; k++)
                sp.data[k] = qint16(quint16(d[k]) << 8);
        }

        const qint32 n = s.data_length;

        if(s.modes & MODES_UNSIGNED)
        {
            for(qint32 k = 0; k < n; k++)
                sp.data[k] = qint16(quint16(sp.data[k]) ^ 0x8000);
        }

        //Reverse reverse loops and pass them off as normal loops.
        if(s.modes & MODES_REVERSE)
        {
            std::reverse(sp.data.begin(), sp.data.begin() + n);

            qint32 t = s.loop_start;
            s.loop_start = n - s.loop_end;
            s.loop_end = n - t;

            s.modes &= ~MODES_REVERSE;
            s.modes |= MODES_LOOPING;
        }

        //ADJUST_SAMPLE_VOLUMES
        if(amp != -1)
            sp.volume = amp / 100.0;
        else
        {
            int maxamp = 0;

            for(qint32 k = 0; k < n; k++)
                maxamp = qMax(maxamp, qAbs(int(sp.data[k])));

            sp.volume = maxamp ? (32768.0 / maxamp) : 0.0;
        }

        s.data_length <<= FRACTION_BITS;
        s.loop_start <<= FRACTION_BITS;
        s.loop_end <<= FRACTION_BITS;

        s.loop_start |= (fractions & 0x0F) << (FRACTION_BITS-4);
        s.loop_end |= ((fractions>>4) & 0x0F) << (FRACTION_BITS-4);

        //Fixed pitch and not looped: resample it now.
        if(s.note_to_use && !(s.modes & MODES_LOOPING))
            PreResample(sp);

        if(strip_tail == 1)
            s.data_length = s.loop_end;

        s.modes &= ~(MODES_16BIT | MODES_UNSIGNED);

        ip.samples.append(sp);
    }

    return true;
}

//Signed 8 bit, normalised to full scale with the gain moved into volume.
static QByteArray ConvertSampleData(GusSample& sp)
{
    gusbank_sample_t& s = sp.s;

    const qint64 n = qMin<qint64>(qint64(s.data_length >> FRACTION_BITS) + 1, qint64(sp.data.size()));

    int maxamp = 0;

    for(qint64 k = 0; k < n; k++)
        maxamp = qMax(maxamp, qAbs(int(sp.data[k])));

    QByteArray out;
    out.resize(n + 2);
    memset(out.data(), 0, out.size());

    double volume = 0.0;

    if(maxamp)
    {
        for(qint64 k = 0; k < n; k++)
            out[k] = char(qBound(-128, int(std::lround(sp.data[k] * 127.0 / maxamp)), 127));

        //The player treats s8 as s16 >> 8.
        volume = sp.volume * maxamp / (256.0 * 127.0);
    }

    s.volume = quint16(qBound(0L, std::lround(volume * 4096.0), 65535L));

    return out;
}

static qint32 PitchRatio(const GusSample& sp)
{
    if(!sp.sample_rate)
        return 0;

    if(sp.s.root_freq <= 0)
        return INT32_MAX;

    double r = std::ldexp(double(sp.sample_rate), FRACTION_BITS + GUSBANK_PITCH_SHIFT) / (double(sp.s.root_freq) * GUSBANK_RATE);

    return qint32(qBound(1.0, std::round(r), double(INT32_MAX)));
}

//Resamples a sample that the player pitches at runtime up by factor, with the
//sound effects' windowed sinc. The player takes the nearest sample, so each
//doubling halves its error, for twice the ROM. Skipped if the longer sample
//would overflow the player's fixed point positions.
static void Oversample(GusSample& sp, int factor)
{
    gusbank_sample_t& s = sp.s;

    if(factor <= 1 || !sp.sample_rate || double(s.data_length) * factor >= 0x40000000)
        return;

    const std::vector<double> in(sp.data.begin(), sp.data.end());
    const std::vector<double> out = SincResample(in, 1.0, factor);

    //Halved so the sinc's overshoot can't clip, and the volume doubled to make up.
    sp.data.resize(out.size());

    for(size_t k = 0; k < out.size(); k++)
        sp.data[k] = qint16(qBound(-32768L, std::lround(out[k] * 0.5), 32767L));

    sp.volume *= 2.0;

    s.data_length *= factor;
    s.loop_start *= factor;
    s.loop_end *= factor;
    sp.sample_rate *= factor;
}

static bool ParseConfig(const QString& cfgPath, ToneEntry tone[128], ToneEntry drum[128])
{
    QFile f(cfgPath);

    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical("Can't open %s", qPrintable(cfgPath));
        return false;
    }

    //-1 = ignore (a bank/drumset other than 0).
    ToneEntry* bank = nullptr;

    while(!f.atEnd())
    {
        QString line = QString::fromLatin1(f.readLine());

        int comment = line.indexOf('#');
        if(comment >= 0)
            line.truncate(comment);

        QStringList w = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if(w.isEmpty())
            continue;

        if(w[0] == "bank" || w[0] == "drumset")
        {
            bool isZero = (w.size() > 1 && w[1].toInt() == 0);

            if(!isZero)
                bank = nullptr;
            else
                bank = (w[0] == "bank") ? tone : drum;

            continue;
        }

        if(w[0] == "dir" || w[0] == "source")
            continue;

        bool ok;
        int i = w[0].toInt(&ok);

        if(!ok || i < 0 || i > 127 || w.size() < 2 || !bank)
            continue;

        ToneEntry& t = bank[i];
        t = ToneEntry();
        t.name = w[1];

        for(int j = 2; j < w.size(); j++)
        {
            QStringList kv = w[j].split('=');

            if(kv.size() != 2)
                continue;

            const QString& k = kv[0];
            const QString& v = kv[1];

            if(k == "amp")
                t.amp = v.toInt();
            else if(k == "note")
                t.note = v.toInt();
            else if(k == "pan")
                t.pan = 0; //Mono output.
            else if(k == "keep")
            {
                if(v == "env")
                    t.strip_envelope = 0;
                else if(v == "loop")
                    t.strip_loop = 0;
            }
            else if(k == "strip")
            {
                if(v == "env")
                    t.strip_envelope = 1;
                else if(v == "loop")
                    t.strip_loop = 1;
                else if(v == "tail")
                    t.strip_tail = 1;
            }
        }
    }

    return true;
}

//Find the programs and drum notes that the type 0 MIDI lumps play.
static void ScanMidi(const QByteArray& midi, bool toneUsed[128], bool drumUsed[128])
{
    const quint8* p = reinterpret_cast<const quint8*>(midi.constData());
    const qint64 size = midi.size();

    if(size < 22 || memcmp(p, "MThd", 4) || memcmp(p + 14, "MTrk", 4))
        return;

    qint64 pos = 22;
    const qint64 end = qMin<qint64>(size, pos + qFromBigEndian<quint32>(p + 18));

    quint8 program[16] = {0};
    quint8 status = 0;

    auto varlen = [&]() { quint32 v = 0; quint8 b; do { b = p[pos++]; v = (v << 7) | (b & 0x7f); } while((b & 0x80) && pos < end); return v; };

    while(pos < end)
    {
        varlen();

        if(pos >= end)
            break;

        quint8 b = p[pos];

        if(b == 0xff)
        {
            pos += 2;
            pos += varlen();
            continue;
        }

        if(b == 0xf0 || b == 0xf7)
        {
            pos++;
            pos += varlen();
            continue;
        }

        if(b & 0x80)
        {
            status = b;
            pos++;
        }

        const quint8 type = status >> 4;
        const quint8 ch = status & 0x0f;

        const quint8 a = p[pos++];

        if(type == 0xc || type == 0xd)
        {
            if(type == 0xc)
                program[ch] = a & 0x7f;

            continue;
        }

        const quint8 v = p[pos++];

        if(type == 0x9 && v)
        {
            if(ch == 9)
                drumUsed[a & 0x7f] = true;
            else
                toneUsed[program[ch]] = true;
        }
    }
}

bool BuildGusBank(WadFile& wadFile, const QString& cfgPath, Lump& bankLump, int oversample)
{
    ToneEntry tone[128], drum[128];

    if(!ParseConfig(cfgPath, tone, drum))
        return false;

    const QDir patchDir = QFileInfo(cfgPath).absoluteDir();

    bool toneUsed[128] = {false}, drumUsed[128] = {false};

    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;
        wadFile.GetLumpByNum(i, l);

        if(l.name.startsWith("D_"))
            ScanMidi(l.data, toneUsed, drumUsed);
    }

    QVector<GusInstrument> instruments;
    QHash<QString, int> loaded;

    gusbank_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "GUSBANK", 8);
    header.version = GUSBANK_VERSION;
    header.rate = GUSBANK_RATE;
    header.control_ratio = GUSBANK_CONTROL_RATIO;

    for(int dr = 0; dr < 2; dr++)
    {
        for(int i = 0; i < 128; i++)
        {
            const ToneEntry& t = dr ? drum[i] : tone[i];

            if(!(dr ? drumUsed[i] : toneUsed[i]) || t.name.isEmpty())
                continue;

            //Same arguments as libtimidity's fill_bank().
            int note = (t.note != -1) ? t.note : (dr ? i : -1);
            int strip_loop = (t.strip_loop != -1) ? t.strip_loop : (dr ? 1 : -1);
            int strip_envelope = (t.strip_envelope != -1) ? t.strip_envelope : (dr ? 1 : -1);

            QString key = QString("%1/%2/%3/%4/%5/%6").arg(t.name).arg(note).arg(t.amp).arg(strip_loop).arg(strip_envelope).arg(t.strip_tail);

            int index = loaded.value(key, -1);

            if(index < 0)
            {
                GusInstrument ip;

                QString path = patchDir.filePath(t.name);
                if(!path.endsWith(".pat", Qt::CaseInsensitive))
                    path += ".pat";

                if(!LoadInstrument(path, ip, t.pan, t.amp, note, strip_loop, strip_envelope, t.strip_tail))
                    continue;

                index = instruments.size();
                instruments.append(ip);
                loaded.insert(key, index);
            }

            if(dr)
                header.drum[i] = quint16(index + 1);
            else
                header.tone[i] = quint16(index + 1);
        }
    }

    header.num_instruments = instruments.size();

    //Layout: header, instrument table, sample tables, sample data.
    quint32 offset = sizeof(gusbank_header_t) + sizeof(gusbank_instrument_t) * instruments.size();

    QVector<gusbank_instrument_t> instrumentTable(instruments.size());

    for(int i = 0; i < instruments.size(); i++)
    {
        instrumentTable[i].num_samples = instruments[i].samples.size();
        instrumentTable[i].samples_offset = offset;
        offset += sizeof(gusbank_sample_t) * instruments[i].samples.size();
    }

    QByteArray sampleData;

    for(int i = 0; i < instruments.size(); i++)
    {
        for(GusSample& sp : instruments[i].samples)
        {
            Oversample(sp, oversample);

            sp.s.pitch_ratio = PitchRatio(sp);
            sp.s.data_offset = offset + sampleData.size();

            sampleData.append(ConvertSampleData(sp));

            while(sampleData.size() & 3)
                sampleData.append(char(0));
        }
    }

    QByteArray out;
    out.append(reinterpret_cast<const char*>(&header), sizeof(header));
    out.append(reinterpret_cast<const char*>(instrumentTable.constData()), sizeof(gusbank_instrument_t) * instrumentTable.size());

    for(int i = 0; i < instruments.size(); i++)
        for(const GusSample& sp : instruments[i].samples)
            out.append(reinterpret_cast<const char*>(&sp.s), sizeof(gusbank_sample_t));

    out.append(sampleData);

    bankLump.name = "GUSBANK";
    bankLump.data = out;
    bankLump.length = out.size();

    int numSamples = 0;
    for(const GusInstrument& ip : instruments)
        numSamples += ip.samples.size();

    qInfo("GUSBANK: %d instruments, %d samples, %d bytes, oversampled %dx.", int(instruments.size()), numSamples, int(out.size()), oversample);

    return true;
}
