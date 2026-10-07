#include "mus2mid.h"

//MUS event types (bits 4-6 of the event byte).
enum
{
    mus_releasekey = 0,
    mus_presskey = 1,
    mus_pitchwheel = 2,
    mus_systemevent = 3,
    mus_changecontroller = 4,
    mus_measureend = 5,
    mus_scoreend = 6
};

//MUS controller -> MIDI controller. MUS controller 0 is a program change.
static const quint8 controllerMap[15] =
{
    0x00, 0x00, 0x01, 0x07, 0x0A, 0x0B, 0x5B, 0x5D,
    0x40, 0x43, 0x78, 0x7B, 0x7E, 0x7F, 0x79
};

class MidiTrackWriter
{
public:
    QByteArray data;
    quint32 delay = 0;
    quint8 runningStatus = 0;

    void WriteVarLen(quint32 value)
    {
        quint8 bytes[5];
        int count = 0;

        bytes[count++] = value & 0x7f;

        while(value >>= 7)
            bytes[count++] = (value & 0x7f) | 0x80;

        while(count--)
            data.append(char(bytes[count]));
    }

    void WriteEvent(quint8 status, quint8 a)
    {
        WriteStatus(status);
        data.append(char(a & 0x7f));
    }

    void WriteEvent(quint8 status, quint8 a, quint8 b)
    {
        WriteStatus(status);
        data.append(char(a & 0x7f));
        data.append(char(b & 0x7f));
    }

    void WriteEndOfTrack()
    {
        WriteVarLen(delay);
        delay = 0;
        runningStatus = 0;
        data.append(char(0xff));
        data.append(char(0x2f));
        data.append(char(0x00));
    }

private:
    void WriteStatus(quint8 status)
    {
        WriteVarLen(delay);
        delay = 0;

        if(status != runningStatus)
        {
            data.append(char(status));
            runningStatus = status;
        }
    }
};

//MUS channel 15 is percussion (MIDI channel 9). Others keep their number, skipping 9.
static quint8 MidiChannel(quint8 musChannel)
{
    if(musChannel == 15)
        return 9;

    return (musChannel < 9) ? musChannel : musChannel + 1;
}

QByteArray ConvertMusToMidi(const QByteArray& mus)
{
    const quint8* p = reinterpret_cast<const quint8*>(mus.constData());
    const int size = mus.size();

    if(size < 16 || memcmp(p, "MUS\x1a", 4) != 0)
        return QByteArray();

    const quint32 scoreLength = qFromLittleEndian<quint16>(p + 4);
    const quint32 scoreStart = qFromLittleEndian<quint16>(p + 6);

    if(scoreStart >= quint32(size))
        return QByteArray();

    const quint32 scoreEnd = qMin(quint32(size), scoreStart + scoreLength);

    MidiTrackWriter track;

    quint8 channelVolume[16];
    bool channelUsed[16];

    for(int i = 0; i < 16; i++)
    {
        channelVolume[i] = 127;
        channelUsed[i] = false;
    }

    quint32 pos = scoreStart;
    bool done = false;

    while(!done && pos < scoreEnd)
    {
        const quint8 eventByte = p[pos++];
        const quint8 type = (eventByte >> 4) & 7;
        const quint8 musChannel = eventByte & 0x0f;
        const quint8 channel = MidiChannel(musChannel);

        //First use of a channel: all notes off. Fixes stuck notes in some songs (D_DDTBLU).
        if(!channelUsed[channel] && type != mus_measureend && type != mus_scoreend)
        {
            channelUsed[channel] = true;
            track.WriteEvent(0xb0 | channel, 0x7b, 0);
        }

        switch(type)
        {
            case mus_releasekey:
            {
                if(pos >= scoreEnd)
                    return QByteArray();

                track.WriteEvent(0x80 | channel, p[pos++], 0);
            }
            break;

            case mus_presskey:
            {
                if(pos >= scoreEnd)
                    return QByteArray();

                quint8 key = p[pos++];

                if(key & 0x80)
                {
                    if(pos >= scoreEnd)
                        return QByteArray();

                    channelVolume[channel] = qMin<quint8>(p[pos++], 127);
                }

                track.WriteEvent(0x90 | channel, key & 0x7f, channelVolume[channel]);
            }
            break;

            case mus_pitchwheel:
            {
                if(pos >= scoreEnd)
                    return QByteArray();

                quint32 bend = quint32(p[pos++]) * 64;

                track.WriteEvent(0xe0 | channel, bend & 0x7f, (bend >> 7) & 0x7f);
            }
            break;

            case mus_systemevent:
            {
                if(pos >= scoreEnd)
                    return QByteArray();

                quint8 controller = p[pos++];

                if(controller >= 10 && controller <= 14)
                    track.WriteEvent(0xb0 | channel, controllerMap[controller], 0);
            }
            break;

            case mus_changecontroller:
            {
                if(pos + 1 >= scoreEnd)
                    return QByteArray();

                quint8 controller = p[pos++];
                quint8 value = qMin<quint8>(p[pos++], 127);

                if(controller == 0)
                    track.WriteEvent(0xc0 | channel, value);
                else if(controller < 10)
                    track.WriteEvent(0xb0 | channel, controllerMap[controller], value);
            }
            break;

            case mus_measureend:
                break;

            case mus_scoreend:
                done = true;
                break;

            default:
                return QByteArray();
        }

        if(eventByte & 0x80)
        {
            quint32 delay = 0;
            quint8 b;

            do
            {
                if(pos >= scoreEnd)
                    return QByteArray();

                b = p[pos++];
                delay = (delay << 7) | (b & 0x7f);
            } while(b & 0x80);

            track.delay += delay;
        }
    }

    track.WriteEndOfTrack();

    //MUS runs at 140 ticks per second: 70 ticks per quarter note at the default 120bpm.
    QByteArray midi;

    midi.append("MThd", 4);
    const char header[10] = {0, 0, 0, 6, 0, 0, 0, 1, 0, 70};
    midi.append(header, sizeof(header));

    midi.append("MTrk", 4);

    char trackLength[4];
    qToBigEndian<quint32>(track.data.size(), trackLength);
    midi.append(trackLength, 4);

    midi.append(track.data);

    return midi;
}
