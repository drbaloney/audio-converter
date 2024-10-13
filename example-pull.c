// Example: Pulling Audio Frames Through Dr. Baloney's Audio Converter.
//
// This example demonstrates how to use Dr. Baloney's Audio Converter library to
// convert audio from a source sample rate at 44.1 kHz to a target sample rate
// at 48 kHz using the pull workflow.
//
// In this workflow, audio frames are requested by the converter from a
// producer callback (`produce_frames`) and are subsequently resampled.
// In this case, the callback generates synthetic audio data (simple incremental
// values for each channel), which is then processed and stored in a buffer.
//
// This example demonstrates the core usage of the library, including memory
// management, resampling between standard sample rates, and handling
// multi-channel audio (stereo).

#include "drb-audio-converter.h"

#include <assert.h> // For `assert`.
#include <stdlib.h> // For `EXIT_SUCCESS`, `aligned_alloc`, and `free`.
#include <stdio.h> // For `printf`.

// Enum values that define the conversion parameters.
enum { source_sampling_rate = drb_audio_converter_sampling_rate_44100 };
enum { target_sampling_rate = drb_audio_converter_sampling_rate_48000 };
enum { channel_count = 2 }; // Stereo (2 channels)
enum { max_frame_count = 256 }; // Maximum number of frames in `consume_frames`.
enum { quality = drb_audio_converter_quality_good }; // Resampling quality.
enum { total_frame_count = 400 }; // Total amount of frames to be processed.

// Defines how many frames will be pushed to the converter in each call.
static int const slices [] = { 13, 17, 4, 7, 5, 4, 21, 29, 300 };

// Calculate the number of slices.
enum { slice_count = sizeof(slices) / sizeof(int) };

// Struct to keep track of how many frames that have been processed.
typedef struct Counter
{
    int count;
}
Counter;

// This function will be called to produce new frames to be converted.
// `user_data`: User-provided data, in this case, a `Counter` instance.
// `latency`: Latency caused by the resampling (in seconds).
// `buffers`: Array of audio buffers; one for each channel.
// `frame_count`: Number of frames that needs to be written in each buffer.
static void produce_frames
    (
        void * user_data,
        double latency,
        DrB_Audio_Converter_Buffer const buffers [],
        int frame_count
    )
{
    // Access the Counter struct to track the total amount of processed frames.

    Counter * const counter = user_data;

    // Print the latency.

    printf("`produce_frames` (latency: %lf)\n", latency);

    // Fill the buffers with sample values of increasing value, starting from
    // the current count.

    int const offset = counter->count;

    for (int channel = 0; channel < channel_count; channel++)
    {
        for (int frame = 0; frame < frame_count; frame++)
        {
            buffers[channel].samples[frame] = frame + offset + 100 * channel;
        }
    }

    // Increment the counter after producing the requested frames.

    counter->count += frame_count;
}

extern int main (int const argc, char const * const argv [const])
{
    (void)argc, (void)argv; // Suppress unused parameter warnings.

    long alignment, size;

    // Calculate the alignment and size required for the converter.

    assert(drb_audio_converter_alignment_and_size
    (
        source_sampling_rate,
        target_sampling_rate,
        channel_count,
        max_frame_count,
        drb_audio_converter_direction_pull,
        quality,
        &alignment,
        &size
    ));

    // Allocate memory for the converter with the correct alignment.

    void * const converter_memory = aligned_alloc(alignment, size);

    // Ensure allocation was successful.

    assert(converter_memory);

    // Initialize the frame counter.

    Counter counter = { .count = 0 };

    // Construct the converter.

    DrB_Audio_Converter * const converter = drb_audio_converter_construct
    (
        converter_memory,
        source_sampling_rate,
        target_sampling_rate,
        channel_count,
        max_frame_count,
        drb_audio_converter_direction_pull,
        quality,
        (DrB_Audio_Converter_Data_Callback){ produce_frames, &counter }
    );

    // Ensure the converter was created successfully.

    assert(converter);

    // Calculate the memory required for the work buffer (temporary buffer used
    // during processing).

    drb_audio_converter_work_memory_alignment_and_size
    (
        converter,
        &alignment,
        &size
    );

    // Allocate the work memory.

    void * const work_memory = aligned_alloc(alignment, size);

    // Ensure allocation was successful.

    assert(work_memory);

    // Prepare an array to hold the resampled data.

    float samples [channel_count][total_frame_count];

    // Process each "slice" (batch of frames) as defined by the `slices` array.

    for (int index = 0, offset = 0; index < slice_count; index++)
    {
        // Declare the buffers and make them point to the corresponding part of
        // the input data.

        DrB_Audio_Converter_Buffer buffers [channel_count];

        for (int channel = 0; channel < channel_count; channel++)
        {
            buffers[channel].samples = samples[channel] + offset;
        }

        // Process the slice.

        drb_audio_converter_process
        (
            converter_memory,
            work_memory,
            buffers,
            slices[index]
        );

        // Update the offset for the next slice of frames.

        offset += slices[index];

        // Ensure we don't exceed the total number of frames.

        assert(offset <= total_frame_count);
    }

    // Print the final resampled data.

    for (int frame = 0; frame < total_frame_count; frame++)
    {
        printf("%3d", frame);

        for (int channel = 0; channel < channel_count; channel++)
        {
            printf(", %8.3f", samples[channel][frame]);
        }

        printf("\n");
    }

    // Free the converter and work memory.

    free(work_memory);
    free(converter_memory);

    // Done!

    return EXIT_SUCCESS;
}