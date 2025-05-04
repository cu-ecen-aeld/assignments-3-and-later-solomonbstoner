/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
	struct aesd_buffer_entry *ret_val = NULL;
	for (uint8_t i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) // make a full round from out_offs
	{
		uint8_t offs = (buffer->out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		size_t str_len = buffer->entry[offs].size;
		if (char_offset < str_len) // size_t is unsigned. No negative numbers allowed
		{
			// JACKPOT
			*entry_offset_byte_rtn = char_offset;
			ret_val = &(buffer->entry[offs]);
			break;
		}
		else
			char_offset -= str_len;
	}
    return ret_val;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    // TODO: Handle memory leak of replacing existing entry if buffer->full == true
	memcpy(&(buffer->entry[buffer->in_offs]), add_entry, sizeof(struct aesd_buffer_entry)); // Add entry regardless of whether its full
	//buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
	//buffer->entry[buffer->in_offs].size = add_entry->size;
	(buffer->in_offs)++;
	buffer->in_offs %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; // circle around
	if (buffer -> full)
	{
		(buffer->out_offs)++; // Update the read ptr if its full
		buffer->out_offs %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; // circle around
	}
	else if (buffer->in_offs == buffer->out_offs)
		buffer->full = true; // buffer is full if write and read offsets are the same
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
