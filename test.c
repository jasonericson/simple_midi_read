#include <string.h>

/*#define SMR_VERBOSE_DEBUGGING*/
#include "simple_midi_read.h"

void print_midi_data(const struct smr_midi_data* midi_data)
{
    int track_index, event_index;

    printf("Format: %d\n", midi_data->format);
    printf("Time Type: ");
    switch(midi_data->time_type)
    {
        case SMR_TT_metrical:
            printf("Metrical.\n");
            printf("- tickdiv = %d\n", midi_data->tickdiv);
            break;
        case SMR_TT_timecode:
            printf("Timecode\n");
            printf("- fps = %d\n", midi_data->fps);
            printf("- subframe_resolution = %d\n", midi_data->subframe_resolution);
            break;
    }

    printf("\n");
    printf("Tracks\n");
    printf("------\n");
    for (track_index = 0; track_index < midi_data->ntracks; track_index++)
    {
        struct smr_track_data* track;

        track = midi_data->tracks + track_index;
        printf("- Track %d:\n", track_index);
        for (event_index = 0; event_index < track->nevents; ++event_index)
        {
            struct smr_event* event;

            event = track->events + event_index;
            printf("  - Event %d | Delta Time = %u\n", event_index, event->delta_time);
            switch(event->event_type)
            {
                case Midi_NoteOff:
                    printf("    - MIDI: Note Off\n");
                    printf("      - note = %d\n", event->note);
                    printf("      - velocity = %d\n", event->velocity);
                    break;
                case Midi_NoteOn:
                    printf("    - MIDI: Note On\n");
                    printf("      - note = %d\n", event->note);
                    printf("      - velocity = %d\n", event->velocity);
                    break;
                case Midi_PolyphonicPressure:
                    printf("    - MIDI: Polyphonic Pressure\n");
                    printf("      - note = %d\n", event->note);
                    printf("      - pressure = %d\n", event->pressure);
                    break;
                case Midi_Controller:
                    printf("    - MIDI: Controller\n");
                    printf("      - controller = %d\n", event->controller);
                    printf("      - value = %d\n", event->value);
                    break;
                case Midi_ProgramChange:
                    printf("    - MIDI: Program Change\n");
                    printf("      - program = %d\n", event->program);
                    break;
                case Midi_ChannelPressure:
                    printf("    - MIDI: Channel Pressure\n");
                    printf("      - pressure = %d\n", event->pressure);
                    break;
                case Midi_PitchBend:
                    printf("    - MIDI: Pitch Bend\n");
                    printf("      - pitch_bend = %d\n", event->pitch_bend);
                    break;
                case SysEx_Single:
                    printf("    - SysEx: Single\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - message = ");
                    {
                        uint32_t msg_index;
                        for (msg_index = 0; msg_index < event->length; ++msg_index)
                        {
                            printf("0x%.2x", event->message[msg_index] & 0xFF);
                        }
                    }
                    printf("\n");
                    break;
                case SysEx_Escape:
                    printf("    - SysEx: Escape\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - message = ");
                    {
                        uint32_t msg_index;
                        for (msg_index = 0; msg_index < event->length; ++msg_index)
                        {
                            printf("0x%.2x", event->message[msg_index] & 0xFF);
                        }
                    }
                    printf("\n");
                    break;
                case Meta_SequenceNumber:
                    printf("    - Meta: Sequence Number\n");
                    printf("      - ss ss = %hu\n", event->ss_ss);
                    break;
                case Meta_Text:
                    printf("    - Meta: Text\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_Copyright:
                    printf("    - Meta: Copyright\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_TrackName:
                    printf("    - Meta: Track Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_InstrumentName:
                    printf("    - Meta: Instrument Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_Lyric:
                    printf("    - Meta: Lyric\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_Marker:
                    printf("    - Meta: Marker\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_CuePoint:
                    printf("    - Meta: Cue Point\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_ProgramName:
                    printf("    - Meta: Program Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_DeviceName:
                    printf("    - Meta: Device Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case Meta_MidiChannelPrefix:
                    printf("    - Meta: MIDI Channel Prefix\n");
                    printf("      - cc = %.2x\n", event->cc & 0xFF);
                    break;
                case Meta_MidiPort:
                    printf("    - Meta: MIDI Port\n");
                    printf("      - pp = %.2x\n", event->pp & 0xFF);
                    break;
                case Meta_EndOfTrack:
                    printf("    - Meta: End of Track\n");
                    break;
                case Meta_Tempo:
                    printf("    - Meta: Tempo\n");
                    printf("      - tempo = %d\n", event->tempo);
                    break;
                case Meta_SmpteOffset:
                    printf("    - Meta: SMPTE Offset\n");
                    printf("      - hr = %.2x\n", event->hr & 0xFF);
                    printf("      - mn = %.2x\n", event->mn & 0xFF);
                    printf("      - se = %.2x\n", event->se & 0xFF);
                    printf("      - fr = %.2x\n", event->fr & 0xFF);
                    printf("      - ff = %.2x\n", event->ff & 0xFF);
                    break;
                case Meta_TimeSignature:
                    printf("    - Meta: Time Signature\n");
                    printf("      - nn = %.2x\n", event->nn & 0xFF);
                    printf("      - dd = %.2x\n", event->dd & 0xFF);
                    printf("      - cc = %.2x\n", event->cc & 0xFF);
                    printf("      - bb = %.2x\n", event->bb & 0xFF);
                    break;
                case Meta_KeySignature:
                    printf("    - Meta: Key Signature\n");
                    printf("      - sf = %.2x\n", event->sf & 0xFF);
                    printf("      - mi = %.2x\n", event->mi & 0xFF);
                    break;
                case Meta_SequencerSpecificEvent:
                    printf("    - Meta: Sequencer Specific Event\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - data = ");
                    {
                        uint32_t data_index;
                        for (data_index = 0; data_index < event->length; ++data_index)
                        {
                            printf("0x%.2x", event->data[data_index] & 0xFF);
                        }
                    }
                    printf("\n");
                    break;
            }
        }
    }
}

int main()
{
    struct smr_midi_data beethoven1, beethoven2, beethoven3;
    int result;

    result = smr_read_file("beethoven1.mid", &beethoven1);
    result = smr_read_file("beethoven2.mid", &beethoven2);
    result = smr_read_file("beethoven3.mid", &beethoven3);
    if (result != 0)
    {
        return result;
    }

    /*print_midi_data(&midi_data);*/

    smr_free_midi_data(&beethoven1);
    smr_free_midi_data(&beethoven2);
    smr_free_midi_data(&beethoven3);
}
