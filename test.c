#include <string.h>

/*#define SMRE_VERBOSE_DEBUGGING*/
#include "simple_midi_read.h"

void print_midi_data(const struct smr_midi_data* midi_data)
{
    int track_index, event_index;

    printf("Format: %d\n", midi_data->format);
    printf("Time Type: ");
    switch(midi_data->time_type)
    {
        case SMRE_metrical:
            printf("Metrical.\n");
            printf("- tickdiv = %d\n", midi_data->tickdiv);
            break;
        case SMRE_timecode:
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
                case SMRE_midi_note_off:
                    printf("    - MIDI: Note Off\n");
                    printf("      - note = %d\n", event->note);
                    printf("      - velocity = %d\n", event->velocity);
                    break;
                case SMRE_midi_note_on:
                    printf("    - MIDI: Note On\n");
                    printf("      - note = %d\n", event->note);
                    printf("      - velocity = %d\n", event->velocity);
                    break;
                case SMRE_midi_polyphonic_pressure:
                    printf("    - MIDI: Polyphonic Pressure\n");
                    printf("      - note = %d\n", event->note);
                    printf("      - pressure = %d\n", event->pressure);
                    break;
                case SMRE_midi_controller:
                    printf("    - MIDI: Controller\n");
                    printf("      - controller = %d\n", event->controller);
                    printf("      - value = %d\n", event->value);
                    break;
                case SMRE_midi_program_change:
                    printf("    - MIDI: Program Change\n");
                    printf("      - program = %d\n", event->program);
                    break;
                case SMRE_midi_channel_pressure:
                    printf("    - MIDI: Channel Pressure\n");
                    printf("      - pressure = %d\n", event->pressure);
                    break;
                case SMRE_midi_pitch_bend:
                    printf("    - MIDI: Pitch Bend\n");
                    printf("      - pitch_bend = %d\n", event->pitch_bend);
                    break;
                case SMRE_sysex_single:
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
                case SMRE_sysex_escape:
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
                case SMRE_meta_sequence_number:
                    printf("    - Meta: Sequence Number\n");
                    printf("      - ss ss = %hu\n", event->ss_ss);
                    break;
                case SMRE_meta_text:
                    printf("    - Meta: Text\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_copyright:
                    printf("    - Meta: Copyright\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_track_name:
                    printf("    - Meta: Track Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_instrument_name:
                    printf("    - Meta: Instrument Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_lyric:
                    printf("    - Meta: Lyric\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_marker:
                    printf("    - Meta: Marker\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_cue_point:
                    printf("    - Meta: Cue Point\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_program_name:
                    printf("    - Meta: Program Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_device_name:
                    printf("    - Meta: Device Name\n");
                    printf("      - length = %d\n", event->length);
                    printf("      - text = %s\n", event->text);
                    break;
                case SMRE_meta_midi_channel_prefix:
                    printf("    - Meta: MIDI Channel Prefix\n");
                    printf("      - cc = %.2x\n", event->cc & 0xFF);
                    break;
                case SMRE_meta_midi_port:
                    printf("    - Meta: MIDI Port\n");
                    printf("      - pp = %.2x\n", event->pp & 0xFF);
                    break;
                case SMRE_meta_end_of_track:
                    printf("    - Meta: End of Track\n");
                    break;
                case SMRE_meta_tempo:
                    printf("    - Meta: Tempo\n");
                    printf("      - tempo = %d\n", event->tempo);
                    break;
                case SMRE_meta_smpte_offset:
                    printf("    - Meta: SMPTE Offset\n");
                    printf("      - hr = %.2x\n", event->hr & 0xFF);
                    printf("      - mn = %.2x\n", event->mn & 0xFF);
                    printf("      - se = %.2x\n", event->se & 0xFF);
                    printf("      - fr = %.2x\n", event->fr & 0xFF);
                    printf("      - ff = %.2x\n", event->ff & 0xFF);
                    break;
                case SMRE_meta_time_signature:
                    printf("    - Meta: Time Signature\n");
                    printf("      - nn = %.2x\n", event->nn & 0xFF);
                    printf("      - dd = %.2x\n", event->dd & 0xFF);
                    printf("      - cc = %.2x\n", event->cc & 0xFF);
                    printf("      - bb = %.2x\n", event->bb & 0xFF);
                    break;
                case SMRE_meta_key_signature:
                    printf("    - Meta: Key Signature\n");
                    printf("      - sf = %.2x\n", event->sf & 0xFF);
                    printf("      - mi = %.2x\n", event->mi & 0xFF);
                    break;
                case SMRE_meta_sequencer_specific_event:
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
    struct smr_midi_data midi_data;
    int result;

    result = smr_read_file("c_scale.mid", &midi_data);
    if (result != 0)
    {
        return result;
    }

    print_midi_data(&midi_data);

    smr_free_midi_data(&midi_data);
}
