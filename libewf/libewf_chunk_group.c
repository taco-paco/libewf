/*
 * Chunk group functions
 *
 * Copyright (c) 2006-2013, Joachim Metz <joachim.metz@gmail.com>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <byte_stream.h>
#include <types.h>

#include "libewf_definitions.h"
#include "libewf_libcerror.h"
#include "libewf_libcnotify.h"
#include "libewf_libfcache.h"
#include "libewf_libfdata.h"
#include "libewf_section.h"

#include "ewf_definitions.h"

/* Fills the chunks list from the EWF version 1 sector table entries
 * Returns 1 if successful or -1 on error
 */
int libewf_chunk_group_fill_v1(
     libfdata_list_t *chunks_list,
     size32_t chunk_size,
     int file_io_pool_entry,
     libewf_section_t *table_section,
     off64_t base_offset,
     uint32_t number_of_entries,
     const uint8_t *table_entries_data,
     size_t table_entries_data_size,
     uint8_t tainted,
     libcerror_error_t **error )
{
	static char *function          = "libewf_chunk_group_fill_v1";
	off64_t last_chunk_data_offset = 0;
	off64_t last_chunk_data_size   = 0;
	off64_t storage_media_offset   = 0;
	uint32_t chunk_data_size       = 0;
	uint32_t current_offset        = 0;
	uint32_t next_offset           = 0;
	uint32_t range_flags           = 0;
	uint32_t stored_offset         = 0;
	uint32_t table_entry_index     = 0;
	uint8_t corrupted              = 0;
	uint8_t is_compressed          = 0;
	uint8_t overflow               = 0;
	int element_index              = 0;

	if( chunks_list == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid chunks list.",
		 function );

		return( -1 );
	}
	if( table_section == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table section.",
		 function );

		return( -1 );
	}
	if( base_offset < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_ZERO_OR_LESS,
		 "%s: invalid base offset.",
		 function );

		return( -1 );
	}
	if( table_entries_data == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table entries data.",
		 function );

		return( -1 );
	}
	if( table_entries_data_size > (size_t) SSIZE_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid table entries data size value exceeds maximum.",
		 function );

		return( -1 );
	}
	byte_stream_copy_to_uint32_little_endian(
	 ( ( (ewf_table_entry_v1_t *) table_entries_data )[ table_entry_index ] ).chunk_data_offset,
	 stored_offset );

	while( table_entry_index < ( number_of_entries - 1 ) )
	{
		if( overflow == 0 )
		{
			is_compressed  = (uint8_t) ( stored_offset >> 31 );
			current_offset = stored_offset & 0x7fffffffUL;
		}
		else
		{
			current_offset = stored_offset;
		}
		byte_stream_copy_to_uint32_little_endian(
		 ( ( (ewf_table_entry_v1_t *) table_entries_data )[ table_entry_index + 1 ] ).chunk_data_offset,
		 stored_offset );

		if( overflow == 0 )
		{
			next_offset = stored_offset & 0x7fffffffUL;
		}
		else
		{
			next_offset = stored_offset;
		}
		corrupted = 0;

		/* This is to compensate for the crappy > 2 GiB segment file solution in EnCase 6.7
		 */
		if( next_offset < current_offset )
		{
			if( stored_offset < current_offset )
			{
#if defined( HAVE_DEBUG_OUTPUT )
				if( libcnotify_verbose != 0 )
				{
					libcnotify_printf(
					 "%s: chunk offset: %" PRIu32 " larger than stored chunk offset: %" PRIu32 ".\n",
					 function,
					 current_offset,
					 stored_offset );
				}
#endif
				corrupted = 1;
			}
#if defined( HAVE_DEBUG_OUTPUT )
			else if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk offset: %" PRIu32 " larger than next chunk offset: %" PRIu32 ".\n",
				 function,
				 current_offset,
				 next_offset );
			}
#endif
			chunk_data_size = stored_offset - current_offset;
		}
		else
		{
			chunk_data_size = next_offset - current_offset;
		}
		if( chunk_data_size == 0 )
		{
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: invalid chunk size value is zero.\n",
				 function );
			}
			corrupted = 1;
		}
		if( chunk_data_size > (uint32_t) INT32_MAX )
		{
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: invalid chunk size value exceeds maximum.\n",
				 function );
			}
			corrupted = 1;
		}
		if( is_compressed != 0 )
		{
			range_flags = LIBEWF_RANGE_FLAG_IS_COMPRESSED;
		}
		else
		{
			range_flags = LIBEWF_RANGE_FLAG_HAS_CHECKSUM;
		}
		if( corrupted != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_CORRUPTED;
		}
		if( tainted != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_TAINTED;
		}
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " base offset\t\t: 0x%08" PRIx64 "\n",
			 function,
			 table_entry_index,
			 base_offset );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data offset\t: 0x%08" PRIx32 "\n",
			 function,
			 table_entry_index,
			 current_offset );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data size\t: %" PRIu32 "\n",
			 function,
			 table_entry_index,
			 chunk_data_size );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data flags:\n",
			 function,
			 table_entry_index );

			if( is_compressed != 0 )
			{
				libcnotify_printf(
				 "Is compressed\n" );
			}
			else
			{
				libcnotify_printf(
				 "Has checksum\n" );
			}
			if( corrupted != 0 )
			{
				libcnotify_printf(
				 "Is corrupted\n" );
			}
			else if( tainted != 0 )
			{
				libcnotify_printf(
				 "Is tainted\n" );
			}
			libcnotify_printf(
			 "\n" );
		}
#endif
		if( libfdata_list_append_element(
		     chunks_list,
		     &element_index,
		     file_io_pool_entry,
		     base_offset + current_offset,
		     (size64_t) chunk_data_size,
		     range_flags,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
			 "%s: unable to append element: %" PRIu32 " to chunks list.",
			 function,
			 table_entry_index );

			return( -1 );
		}
		if( libfdata_list_set_mapped_range_by_index(
		     chunks_list,
		     element_index,
		     storage_media_offset,
		     chunk_size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
			 "%s: unable to set mapped range of element: %d in chunks list.",
			 function,
			 element_index );

			return( -1 );
		}
		storage_media_offset += chunk_size;

		/* This is to compensate for the crappy > 2 GiB segment file solution in EnCase 6.7
		 */
		if( ( overflow == 0 )
		 && ( ( current_offset + chunk_data_size ) > (uint32_t) INT32_MAX ) )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk offset overflow at: %" PRIu32 ".\n",
				 function,
				 current_offset );
			}
#endif
			overflow      = 1;
			is_compressed = 0;
		}
		table_entry_index++;
	}
	byte_stream_copy_to_uint32_little_endian(
	 ( ( (ewf_table_entry_v1_t *) table_entries_data )[ table_entry_index ] ).chunk_data_offset,
	 stored_offset );

	if( overflow == 0 )
	{
		is_compressed  = (uint8_t) ( stored_offset >> 31 );
		current_offset = stored_offset & 0x7fffffffUL;
	}
	else
	{
		current_offset = stored_offset;
	}
	corrupted = 0;

	/* There is no indication how large the last chunk is.
	 * The only thing known is where it starts.
	 * However it can be determined using the offset of the next section.
	 * The size of the last chunk is determined by subtracting the last offset from the offset of the next section.
	 *
	 * The offset of the next section is either table_section->end_offset for original EWF and EWF-S01
	 * or table_section->start_offset for other types of EWF
	 */
	last_chunk_data_offset = (off64_t) base_offset + current_offset;

	if( last_chunk_data_offset > (off64_t) INT64_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid last chunk offset value exceeds maximum.",
		 function );

		return( -1 );
	}
	if( last_chunk_data_offset < table_section->start_offset )
	{
		last_chunk_data_size = table_section->start_offset - last_chunk_data_offset;
	}
	else if( last_chunk_data_offset < table_section->end_offset )
	{
		last_chunk_data_size = table_section->end_offset - last_chunk_data_offset;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	else if( libcnotify_verbose != 0 )
	{
		libcnotify_printf(
		 "%s: invalid last chunk offset value exceeds table section end offset.\n",
		 function );
	}
#endif
	if( last_chunk_data_size <= 0 )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: invalid last chunk size value is zero or less.\n",
			 function );
		}
#endif
		corrupted = 1;
	}
	if( last_chunk_data_size > (off64_t) INT32_MAX )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: invalid last chunk size value exceeds maximum.\n",
			 function );
		}
#endif
		corrupted = 1;
	}
	if( is_compressed != 0 )
	{
		range_flags = LIBEWF_RANGE_FLAG_IS_COMPRESSED;
	}
	else
	{
		range_flags = LIBEWF_RANGE_FLAG_HAS_CHECKSUM;
	}
	if( corrupted != 0 )
	{
		range_flags |= LIBEWF_RANGE_FLAG_IS_CORRUPTED;
	}
	if( tainted != 0 )
	{
		range_flags |= LIBEWF_RANGE_FLAG_IS_TAINTED;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libcnotify_verbose != 0 )
	{
		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " base offset\t\t: 0x%08" PRIx64 "\n",
		 function,
		 table_entry_index,
		 base_offset );

		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " chunk data offset\t: 0x%08" PRIx32 "\n",
		 function,
		 table_entry_index,
		 current_offset );

		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " chunk data size\t: %" PRIu32 " (calculated)\n",
		 function,
		 table_entry_index,
		 last_chunk_data_size );

		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " chunk data flags:\n",
		 function,
		 table_entry_index );

		if( is_compressed != 0 )
		{
			libcnotify_printf(
			 "Is compressed\n" );
		}
		else
		{
			libcnotify_printf(
			 "Has checksum\n" );
		}
		if( corrupted != 0 )
		{
			libcnotify_printf(
			 "Is corrupted\n" );
		}
		else if( tainted != 0 )
		{
			libcnotify_printf(
			 "Is tainted\n" );
		}
		libcnotify_printf(
		 "\n" );
	}
#endif
	if( libfdata_list_append_element(
	     chunks_list,
	     &element_index,
	     file_io_pool_entry,
	     last_chunk_data_offset,
	     (size64_t) last_chunk_data_size,
	     range_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to append element: %" PRIu32 " to chunks list.",
		 function,
		 table_entry_index );

		return( -1 );
	}
	if( libfdata_list_set_mapped_range_by_index(
	     chunks_list,
	     element_index,
	     storage_media_offset,
	     chunk_size,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to set mapped range of element: %d in chunks list.",
		 function,
		 element_index );

		return( -1 );
	}
	storage_media_offset += chunk_size;

#if defined( HAVE_DEBUG_OUTPUT )
	if( libcnotify_verbose != 0 )
	{
		libcnotify_printf(
		 "\n" );
	}
#endif
	return( 1 );
}

/* Fills the chunks list from the EWF version 2 sector table entries
 * Returns 1 if successful or -1 on error
 */
int libewf_chunk_group_fill_v2(
     libfdata_list_t *chunks_list,
     size32_t chunk_size,
     int file_io_pool_entry,
     libewf_section_t *table_section,
     uint32_t number_of_offsets,
     const uint8_t *table_entries_data,
     size_t table_entries_data_size,
     uint8_t tainted,
     libcerror_error_t **error )
{
	static char *function        = "libewf_chunk_group_fill_v2";
	off64_t storage_media_offset = 0;
	off64_t table_entry_offset   = 0;
	uint64_t chunk_data_offset   = 0;
	uint32_t chunk_data_flags    = 0;
	uint32_t chunk_data_size     = 0;
	uint32_t range_flags         = 0;
	uint32_t table_entry_index   = 0;
	int element_index            = 0;

	if( chunks_list == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid chunks list.",
		 function );

		return( -1 );
	}
	if( table_section == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table section.",
		 function );

		return( -1 );
	}
	if( table_entries_data == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table entries data.",
		 function );

		return( -1 );
	}
	if( table_entries_data_size > (size_t) SSIZE_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid table entries data size value exceeds maximum.",
		 function );

		return( -1 );
	}
	table_entry_offset = table_section->start_offset + sizeof( ewf_table_header_v2_t );

	while( table_entries_data_size >= sizeof( ewf_table_entry_v2_t ) )
	{
		byte_stream_copy_to_uint64_little_endian(
		 ( (ewf_table_entry_v2_t *) table_entries_data )->chunk_data_offset,
		 chunk_data_offset );

		byte_stream_copy_to_uint32_little_endian(
		 ( (ewf_table_entry_v2_t *) table_entries_data )->chunk_data_size,
		 chunk_data_size );

		byte_stream_copy_to_uint32_little_endian(
		 ( (ewf_table_entry_v2_t *) table_entries_data )->chunk_data_flags,
		 chunk_data_flags );

#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			if( ( ( chunk_data_flags & LIBEWF_CHUNK_DATA_FLAG_IS_COMPRESSED ) != 0 )
			 && ( ( chunk_data_flags & LIBEWF_CHUNK_DATA_FLAG_USES_PATTERN_FILL ) != 0 ) )
			{
				libcnotify_printf(
				 "%s: table entry: % 8" PRIu32 " chunk pattern fill\t: 0x%08" PRIx64 "\n",
				 function,
				 table_entry_index,
				 chunk_data_offset );
			}
			else
			{
				libcnotify_printf(
				 "%s: table entry: % 8" PRIu32 " chunk data offset\t: 0x%08" PRIx64 "\n",
				 function,
				 table_entry_index,
				 chunk_data_offset );
			}
			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data size\t: %" PRIu32 "\n",
			 function,
			 table_entry_index,
			 chunk_data_size );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data flags\t: 0x%08" PRIx32 "\n",
			 function,
			 table_entry_index,
			 chunk_data_flags );
		}
#endif
		table_entries_data      += sizeof( ewf_table_entry_v2_t );
		table_entries_data_size -= sizeof( ewf_table_entry_v2_t );

		range_flags = 0;

		if( ( chunk_data_flags & LIBEWF_CHUNK_DATA_FLAG_IS_COMPRESSED ) != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_COMPRESSED;

			if( ( chunk_data_flags & LIBEWF_CHUNK_DATA_FLAG_USES_PATTERN_FILL ) != 0 )
			{
				range_flags |= LIBEWF_RANGE_FLAG_USES_PATTERN_FILL;
			}
		}
		if( ( chunk_data_flags & LIBEWF_CHUNK_DATA_FLAG_HAS_CHECKSUM ) != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_HAS_CHECKSUM;
		}
/* TODO handle corruption e.g. check for zero data
		if( ( ( chunk_data_flags & 0x00000007UL ) != 1 )
		 || ( ( chunk_data_flags & 0x00000007UL ) != 2 )
		 || ( ( chunk_data_flags & 0x00000007UL ) != 5 ) )
		{
		}
*/
#if defined( HAVE_VERBOSE_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			if( ( chunk_data_flags & ~( 0x00000007UL ) ) != 0 )
			{
				libcnotify_printf(
				 "%s: unsupported chunk data flags: 0x%08" PRIx32 " in table entry: %" PRIu32 "\n",
				 function,
				 chunk_data_flags,
				 table_entry_index );
			}
		}
#endif
		if( tainted != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_TAINTED;
		}
		if( ( range_flags & LIBEWF_RANGE_FLAG_USES_PATTERN_FILL ) != 0 )
		{
			chunk_data_offset = table_entry_offset;
			chunk_data_size   = 8;
		}
		table_entry_offset += sizeof( ewf_table_entry_v2_t );

		if( libfdata_list_append_element(
		     chunks_list,
		     &element_index,
		     file_io_pool_entry,
		     chunk_data_offset,
		     (size64_t) chunk_data_size,
		     range_flags,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
			 "%s: unable to append element: %" PRIu32 " to chunks list.",
			 function,
			 table_entry_index );

			return( -1 );
		}
		if( libfdata_list_set_mapped_range_by_index(
		     chunks_list,
		     element_index,
		     storage_media_offset,
		     chunk_size,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_APPEND_FAILED,
			 "%s: unable to set mapped range of element: %d in chunks list.",
			 function,
			 element_index );

			return( -1 );
		}
		storage_media_offset += chunk_size;

		table_entry_index++;

#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "\n" );
		}
#endif
	}
	return( 1 );
}

/* Corrects the chunks list from the offsets
 * Returns 1 if successful or -1 on error
 */
int libewf_chunk_group_correct_v1(
     libfdata_list_t *chunks_list,
     size32_t chunk_size,
     int file_io_pool_entry,
     libewf_section_t *table_section,
     off64_t base_offset,
     uint32_t number_of_entries,
     const uint8_t *table_entries_data,
     size_t table_entries_data_size,
     uint8_t tainted,
     libcerror_error_t **error )
{
	static char *function              = "libewf_chunk_group_correct_v1";
	off64_t last_chunk_data_offset     = 0;
	off64_t last_chunk_data_size       = 0;
	off64_t previous_chunk_data_offset = 0;
	size64_t previous_chunk_data_size  = 0;
	uint32_t chunk_data_size           = 0;
	uint32_t current_offset            = 0;
	uint32_t next_offset               = 0;
	uint32_t previous_range_flags      = 0;
	uint32_t range_flags               = 0;
	uint32_t stored_offset             = 0;
	uint32_t table_entry_index         = 0;
	uint8_t corrupted                  = 0;
	uint8_t is_compressed              = 0;
	uint8_t mismatch                   = 0;
	uint8_t overflow                   = 0;
	uint8_t update_data_range          = 0;
	int previous_file_io_pool_entry    = 0;

	if( chunks_list == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid chunks list.",
		 function );

		return( -1 );
	}
	if( table_section == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table section.",
		 function );

		return( -1 );
	}
	if( base_offset < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_ZERO_OR_LESS,
		 "%s: invalid base offset.",
		 function );

		return( -1 );
	}
	if( table_entries_data == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table entries data.",
		 function );

		return( -1 );
	}
	if( table_entries_data_size > (size_t) SSIZE_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid table entries data size value exceeds maximum.",
		 function );

		return( -1 );
	}
	byte_stream_copy_to_uint32_little_endian(
	 ( ( (ewf_table_entry_v1_t *) table_entries_data )[ table_entry_index ] ).chunk_data_offset,
	 stored_offset );

	while( table_entry_index < ( number_of_entries - 1 ) )
	{
		if( overflow == 0 )
		{
			is_compressed  = (uint8_t) ( stored_offset >> 31 );
			current_offset = stored_offset & 0x7fffffffUL;
		}
		else
		{
			current_offset = stored_offset;
		}
		byte_stream_copy_to_uint32_little_endian(
		 ( ( (ewf_table_entry_v1_t *) table_entries_data )[ table_entry_index + 1 ] ).chunk_data_offset,
		 stored_offset );

		if( overflow == 0 )
		{
			next_offset = stored_offset & 0x7fffffffUL;
		}
		else
		{
			next_offset = stored_offset;
		}
		corrupted = 0;

		/* This is to compensate for the crappy > 2 GiB segment file solution in EnCase 6.7
		 */
		if( next_offset < current_offset )
		{
			if( stored_offset < current_offset )
			{
#if defined( HAVE_DEBUG_OUTPUT )
				if( libcnotify_verbose != 0 )
				{
					libcnotify_printf(
					 "%s: chunk offset: %" PRIu32 " larger than stored chunk offset: %" PRIu32 ".\n",
					 function,
					 current_offset,
					 stored_offset );
				}
#endif
				corrupted = 1;
			}
#if defined( HAVE_DEBUG_OUTPUT )
			else if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk offset: %" PRIu32 " larger than next chunk offset: %" PRIu32 ".\n",
				 function,
				 current_offset,
				 next_offset );
			}
#endif
			chunk_data_size = stored_offset - current_offset;
		}
		else
		{
			chunk_data_size = next_offset - current_offset;
		}
		if( chunk_data_size == 0 )
		{
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: invalid chunk size value is zero.\n",
				 function );
			}
			corrupted = 1;
		}
		if( chunk_data_size > (uint32_t) INT32_MAX )
		{
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: invalid chunk size value exceeds maximum.\n",
				 function );
			}
			corrupted = 1;
		}
		range_flags = LIBEWF_RANGE_FLAG_HAS_CHECKSUM;

		if( is_compressed != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_COMPRESSED;
		}
		if( corrupted != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_CORRUPTED;
		}
		if( tainted != 0 )
		{
			range_flags |= LIBEWF_RANGE_FLAG_IS_TAINTED;
		}
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " base offset\t\t: 0x%08" PRIx64 "\n",
			 function,
			 table_entry_index,
			 base_offset );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data offset\t: 0x%08" PRIx32 "\n",
			 function,
			 table_entry_index,
			 current_offset );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data size\t: %" PRIu32 "\n",
			 function,
			 table_entry_index,
			 chunk_data_size );

			libcnotify_printf(
			 "%s: table entry: % 8" PRIu32 " chunk data flags:\n",
			 function,
			 table_entry_index );

			if( is_compressed != 0 )
			{
				libcnotify_printf(
				 "Is compressed\n" );
			}
			else
			{
				libcnotify_printf(
				 "Has checksum\n" );
			}
			if( corrupted != 0 )
			{
				libcnotify_printf(
				 "Is corrupted\n" );
			}
			else if( tainted != 0 )
			{
				libcnotify_printf(
				 "Is tainted\n" );
			}
			libcnotify_printf(
			 "\n" );
		}
#endif
		if( libfdata_list_get_element_by_index(
		     chunks_list,
		     table_entry_index,
		     &previous_file_io_pool_entry,
		     &previous_chunk_data_offset,
		     &previous_chunk_data_size,
		     &previous_range_flags,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve element: %" PRIu32 " from chunks list.",
			 function,
			 table_entry_index );

			return( -1 );
		}
		if( (off64_t) ( base_offset + current_offset ) != previous_chunk_data_offset )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk: %" PRIu32 " offset mismatch.\n",
				 function,
				 table_entry_index );
			}
#endif
			mismatch = 1;
		}
		else if( (size64_t) chunk_data_size != previous_chunk_data_size )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk: %" PRIu32 " size mismatch.\n",
				 function,
				 table_entry_index );
			}
#endif
			mismatch = 1;
		}
		else if( ( range_flags & LIBEWF_RANGE_FLAG_IS_COMPRESSED )
		      != ( previous_range_flags & LIBEWF_RANGE_FLAG_IS_COMPRESSED ) )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk: %" PRIu32 " compression flag mismatch.\n",
				 function,
				 table_entry_index );
			}
#endif
			mismatch = 1;
		}
		else
		{
			mismatch = 0;
		}
		update_data_range = 0;

		if( mismatch != 0 )
		{
			if( ( corrupted == 0 )
			 && ( tainted == 0 ) )
			{
				update_data_range = 1;
			}
			else if( ( ( previous_range_flags & LIBEWF_RANGE_FLAG_IS_CORRUPTED ) != 0 )
			      && ( corrupted == 0 ) )
			{
				update_data_range = 1;
			}
		}
		else if( ( previous_range_flags & LIBEWF_RANGE_FLAG_IS_TAINTED ) != 0 )
		{
			update_data_range = 1;
		}
		if( update_data_range != 0 )
		{
			if( libfdata_list_set_element_by_index(
			     chunks_list,
			     table_entry_index,
			     file_io_pool_entry,
			     base_offset + current_offset,
			     (size64_t) chunk_data_size,
			     range_flags,
			     error ) != 1 )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
				 "%s: unable to set element: %" PRIu32 " in chunks list.",
				 function,
				 table_entry_index );

				return( -1 );
			}
		}
		/* This is to compensate for the crappy > 2 GiB segment file solution in EnCase 6.7
		 */
		if( ( overflow == 0 )
		 && ( ( current_offset + chunk_data_size ) > (uint32_t) INT32_MAX ) )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( libcnotify_verbose != 0 )
			{
				libcnotify_printf(
				 "%s: chunk offset overflow at: %" PRIu32 ".\n",
				 function,
				 current_offset );
			}
#endif
			overflow      = 1;
			is_compressed = 0;
		}
		table_entry_index++;
	}
	byte_stream_copy_to_uint32_little_endian(
	 ( ( (ewf_table_entry_v1_t *) table_entries_data )[ table_entry_index ] ).chunk_data_offset,
	 stored_offset );

	if( overflow == 0 )
	{
		is_compressed  = (uint8_t) ( stored_offset >> 31 );
		current_offset = stored_offset & 0x7fffffffUL;
	}
	else
	{
		current_offset = stored_offset;
	}
	corrupted = 0;

	/* There is no indication how large the last chunk is.
	 * The only thing known is where it starts.
	 * However it can be determined using the offset of the next section.
	 * The size of the last chunk is determined by subtracting the last offset from the offset of the next section.
	 *
	 * The offset of the next section is either table_section->end_offset for original EWF and EWF-S01
	 * or table_section->start_offset for other types of EWF
	 */
	last_chunk_data_offset = (off64_t) base_offset + current_offset;

	if( last_chunk_data_offset > (off64_t) INT64_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid last chunk offset value exceeds maximum.",
		 function );

		return( -1 );
	}
	if( last_chunk_data_offset < table_section->start_offset )
	{
		last_chunk_data_size = table_section->start_offset - last_chunk_data_offset;
	}
	else if( last_chunk_data_offset < table_section->end_offset )
	{
		last_chunk_data_size = table_section->end_offset - last_chunk_data_offset;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	else if( libcnotify_verbose != 0 )
	{
		libcnotify_printf(
		 "%s: invalid last chunk offset value exceeds table section end offset.\n",
		 function );
	}
#endif
	last_chunk_data_size -= table_section->size;

	if( last_chunk_data_size <= 0 )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: invalid last chunk size value is zero or less.\n",
			 function );
		}
#endif
		corrupted = 1;
	}
	if( last_chunk_data_size > (off64_t) INT32_MAX )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: invalid last chunk size value exceeds maximum.\n",
			 function );
		}
#endif
		corrupted = 1;
	}
	range_flags = LIBEWF_RANGE_FLAG_HAS_CHECKSUM;

	if( is_compressed != 0 )
	{
		range_flags |= LIBEWF_RANGE_FLAG_IS_COMPRESSED;
	}
	if( corrupted != 0 )
	{
		range_flags |= LIBEWF_RANGE_FLAG_IS_CORRUPTED;
	}
	if( tainted != 0 )
	{
		range_flags |= LIBEWF_RANGE_FLAG_IS_TAINTED;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libcnotify_verbose != 0 )
	{
		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " base offset\t\t: 0x%08" PRIx64 "\n",
		 function,
		 table_entry_index,
		 base_offset );

		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " chunk data offset\t: 0x%08" PRIx32 "\n",
		 function,
		 table_entry_index,
		 current_offset );

		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " chunk data size\t: %" PRIu32 " (calculated)\n",
		 function,
		 table_entry_index,
		 last_chunk_data_size );

		libcnotify_printf(
		 "%s: table entry: % 8" PRIu32 " chunk data flags:\n",
		 function,
		 table_entry_index );

		if( is_compressed != 0 )
		{
			libcnotify_printf(
			 "Is compressed\n" );
		}
		else
		{
			libcnotify_printf(
			 "Has checksum\n" );
		}
		if( corrupted != 0 )
		{
			libcnotify_printf(
			 "Is corrupted\n" );
		}
		else if( tainted != 0 )
		{
			libcnotify_printf(
			 "Is tainted\n" );
		}
		libcnotify_printf(
		 "\n" );
	}
#endif
	if( libfdata_list_get_element_by_index(
	     chunks_list,
	     table_entry_index,
	     &previous_file_io_pool_entry,
	     &previous_chunk_data_offset,
	     &previous_chunk_data_size,
	     &previous_range_flags,
	     error ) != 1 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve element: %" PRIu32 " from chunks list.",
		 function,
		 table_entry_index );

		return( -1 );
	}
	if( (off64_t) ( base_offset + current_offset ) != previous_chunk_data_offset )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: chunk: %" PRIu32 " offset mismatch.\n",
			 function,
			 table_entry_index );
		}
#endif
		mismatch = 1;
	}
	else if( (size64_t) chunk_data_size != previous_chunk_data_size )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: chunk: %" PRIu32 " size mismatch.\n",
			 function,
			 table_entry_index );
		}
#endif
		mismatch = 1;
	}
	else if( ( range_flags & LIBEWF_RANGE_FLAG_IS_COMPRESSED )
	      != ( previous_range_flags & LIBEWF_RANGE_FLAG_IS_COMPRESSED ) )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libcnotify_verbose != 0 )
		{
			libcnotify_printf(
			 "%s: chunk: %" PRIu32 " compression flag mismatch.\n",
			 function,
			 table_entry_index );
		}
#endif
		mismatch = 1;
	}
	else
	{
		mismatch = 0;
	}
	update_data_range = 0;

	if( mismatch != 0 )
	{
		if( ( corrupted == 0 )
		 && ( tainted == 0 ) )
		{
			update_data_range = 1;
		}
		else if( ( ( previous_range_flags & LIBEWF_RANGE_FLAG_IS_CORRUPTED ) != 0 )
		      && ( corrupted == 0 ) )
		{
			update_data_range = 1;
		}
	}
	else if( ( previous_range_flags & LIBEWF_RANGE_FLAG_IS_TAINTED ) != 0 )
	{
		update_data_range = 1;
	}
	if( update_data_range != 0 )
	{
		if( libfdata_list_set_element_by_index(
		     chunks_list,
		     table_entry_index,
		     file_io_pool_entry,
		     base_offset + current_offset,
		     (size64_t) chunk_data_size,
		     range_flags,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set element: %" PRIu32 " in chunks list.",
			 function,
			 table_entry_index );

			return( -1 );
		}
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libcnotify_verbose != 0 )
	{
		libcnotify_printf(
		 "\n" );
	}
#endif
	return( 1 );
}

/* Generates the table entries data from the chunks list
 * Returns 1 if successful or -1 on error
 */
int libewf_chunk_group_generate_table_entries_data(
     libfdata_list_t *chunks_list,
     uint8_t format_version,
     uint8_t *table_entries_data,
     size_t table_entries_data_size,
     uint32_t number_of_entries,
     off64_t base_offset,
     libcerror_error_t **error )
{
	static char *function        = "libewf_chunk_group_generate_table_entries_data";
	off64_t chunk_data_offset    = 0;
	size64_t chunk_data_size     = 0;
	size_t table_entry_data_size = 0;
	uint32_t chunk_data_flags    = 0;
	uint32_t range_flags         = 0;
	uint32_t table_offset        = 0;
	uint32_t table_entry_index   = 0;
	int file_io_pool_entry       = 0;

	if( chunks_list == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid chunks list.",
		 function );

		return( -1 );
	}
	if( format_version == 1 )
	{
		table_entry_data_size = sizeof( ewf_table_entry_v1_t );
	}
	else if( format_version == 2 )
	{
		table_entry_data_size = sizeof( ewf_table_entry_v2_t );
	}
	else
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported format version.",
		 function );

		return( -1 );
	}
	if( table_entries_data == NULL )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid table entries data.",
		 function );

		return( -1 );
	}
	if( table_entries_data_size > (size_t) SSIZE_MAX )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid table entries data size value exceeds maximum.",
		 function );

		return( -1 );
	}
	if( ( number_of_entries * table_entry_data_size ) > table_entries_data_size )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid table entries data size value out of bounds.",
		 function );

		return( -1 );
	}
	if( base_offset < 0 )
	{
		libcerror_error_set(
		 error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_ZERO_OR_LESS,
		 "%s: invalid base offset.",
		 function );

		return( -1 );
	}
	for( table_entry_index = 0;
	     table_entry_index < number_of_entries;
	     table_entry_index++ )
	{
		if( libfdata_list_get_element_by_index(
		     chunks_list,
		     table_entry_index,
		     &file_io_pool_entry,
		     &chunk_data_offset,
		     &chunk_data_size,
		     &range_flags,
		     error ) != 1 )
		{
			libcerror_error_set(
			 error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable to retrieve element: %" PRIu32 " from chunks list.",
			 function,
			 table_entry_index );

			return( -1 );
		}
		if( format_version == 1 )
		{
			chunk_data_offset -= base_offset;

			if( ( chunk_data_offset < 0 )
			 || ( chunk_data_offset > (off64_t) INT32_MAX ) )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid chunk: %" PRIu32 " offset value out of bounds.",
				 function,
				 table_entry_index );

				return( -1 );
			}
			table_offset = (uint32_t) chunk_data_offset;

			if( ( range_flags & LIBEWF_RANGE_FLAG_IS_COMPRESSED ) != 0 )
			{
				table_offset |= 0x80000000UL;
			}
			byte_stream_copy_from_uint32_little_endian(
			 ( (ewf_table_entry_v1_t *) table_entries_data )->chunk_data_offset,
			 table_offset );
		}
		else if( format_version == 2 )
		{
			if( chunk_data_size > (size64_t) UINT32_MAX )
			{
				libcerror_error_set(
				 error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid chunk: %" PRIu32 " size value out of bounds.",
				 function,
				 table_entry_index );

				return( -1 );
			}
			chunk_data_flags = 0;

			if( ( range_flags & LIBEWF_RANGE_FLAG_IS_COMPRESSED ) != 0 )
			{
				chunk_data_flags |= LIBEWF_CHUNK_DATA_FLAG_IS_COMPRESSED;
			}
			if( ( range_flags & LIBEWF_RANGE_FLAG_HAS_CHECKSUM ) != 0 )
			{
				chunk_data_flags |= LIBEWF_CHUNK_DATA_FLAG_HAS_CHECKSUM;
			}
			if( ( range_flags & LIBEWF_RANGE_FLAG_USES_PATTERN_FILL ) != 0 )
			{
				chunk_data_flags |= LIBEWF_CHUNK_DATA_FLAG_USES_PATTERN_FILL;
			}
			byte_stream_copy_from_uint64_little_endian(
			 ( (ewf_table_entry_v2_t *) table_entries_data )->chunk_data_offset,
			 chunk_data_offset );

			byte_stream_copy_from_uint32_little_endian(
			 ( (ewf_table_entry_v2_t *) table_entries_data )->chunk_data_size,
			 (uint32_t) chunk_data_size );

			byte_stream_copy_from_uint32_little_endian(
			 ( (ewf_table_entry_v2_t *) table_entries_data )->chunk_data_flags,
			 chunk_data_flags );
		}
		table_entries_data += table_entry_data_size;
	}
	return( 1 );
}
