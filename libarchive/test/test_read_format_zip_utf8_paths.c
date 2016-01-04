/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

/*
 * This collection of tests tries to verify that libarchive correctly
 * handles Zip UTF-8 filenames stored in various fashions, including
 * boundary cases where the different copies of the filename don't
 * agree with each other.
 *
 * A UTF8 filename can appear in a Zip file in three different fashions.
 *
 * Unmarked: If bit 11 of the GP bit flag is not set, then the
 * filename is stored in an unspecified encoding which may or may not
 * be UTF-8.  Practically speaking, decoders can make no assumptions
 * about the filename encoding.
 *
 * GP bit flag #11:  If this bit is set, then the Filename and File
 * comment should be stored in UTF-8.
 *
 * Extra field 0x7075: This field was added by Info-ZIP.  It stores a
 * second copy of the filename in UTF-8.  Note this second filename
 * may not be the same encoding -- or even the same name -- as the primary
 * filename.  It makes no assertion about the character set used by
 * the file comment.
 *
 * Also note that the above can appear in the local file header or the
 * central directory or both and may or may not agree in any of those
 * cases.  In the worst case, we may have four different filenames for
 * a single entry: The local file header can have both a regular filename
 * (in UTF-8 or not) and the 0x7075 extension, the central directory
 * would also have both, and all four names could be different.
 */

/*
 * Case 1: Use GP#11 to flag UTF-8 filename in local file header,
 * but central directory has a different name.
 */
static const unsigned char case1[] = {
	/* Local file header */
	0x50, 0x4b, 0x03, 0x04, /* PK\003\004 */
	0x20, 0x00, /* Version needed to extract: 2.0 */
	0x00, 0x08, /* General purpose bit flag: 0x0800 == UTF8 filename */
	0x00, 0x00, /* Compression method: None */
	0x00, 0x00, /* Last mod time */
	0x00, 0x00, /* Last mod date */
	0x00, 0x00, 0x00, 0x00, /* CRC32 */
	0x04, 0x00, 0x00, 0x00, /* Compressed size: 4 */
	0x04, 0x00, 0x00, 0x00, /* Uncompressed size: 4 */
	0x0a, 0x00, /* Filename length: 5 */
	0x00, 0x00, /* Extra field lenght: 0 */
	0x41, 0x42, 0x43, 0xE2, 0x86, 0x92, 0x2e, 0x74, 0x78, 0x74, /* Filename: ABC<right arrow>.txt */
	/* Extra field: Not present */
	
	/* File data */
	0x41, 0x42, 0x43, 0x0a, /* "ABC\n" */

	/* Central directory header */
	0x50, 0x4b, 0x01, 0x02, /* PK\001\002 */
	0x20, 0x00, /* Version made by: 2.0 for MSDOS */
	0x20, 0x00, /* Version needed to extract: 2.0 */
	0x00, 0x08, /* General purpose bit flag: bit 11 = UTF8 filename */
	0x00, 0x00, /* Compression method: None */
	0x00, 0x00, /* Last mod time */
	0x00, 0x00, /* Last mod date */
	0x00, 0x00, 0x00, 0x00, /* CRC32 */
	0x04, 0x00, 0x00, 0x00, /* Compressed size: 4 */
	0x04, 0x00, 0x00, 0x00, /* Uncompressed size: 4 */
	0x05, 0x00, /* Filename length */
	0x00, 0x00, /* Extra field length: 0 */
	0x00, 0x00, /* Comment length: 0 */
	0x00, 0x00, /* Disk number start: 0 */
	0x00, 0x00, /* Internal file attributes */
	0x00, 0x00, 0x00, 0x00, /* External file attributes */
	0x00, 0x00, 0x00, 0x00, /* Offset of local header */
	0x41, 0x2e, 0x74, 0x78, 0x74, /* File name */
	/* Extra field: not present */
	/* File comment: not present */

	/* End of central directory record */
	0x50, 0x4b, 0x05, 0x06, /* PK\005\006 */
	0x00, 0x00, /* Number of this disk: 0 */
	0x00, 0x00, /* Central directory starts on this disk: 0 */
	0x01, 0x00, /* Total CD entries on this disk: 1 */
	0x01, 0x00, /* Total CD entries: 1 */
	0x33, 0x00, 0x00, 0x00, /* Size of CD in bytes */
	0x2c, 0x00, 0x00, 0x00, /* Offset of start of CD */
	0x00, 0x00, /* Length of archive comment: 0 */
	/* Archive comment: not present */
};

DEFINE_TEST(test_read_format_zip_utf8_paths_case1_seeking)
{
	struct archive *a;
	struct archive_entry *ae;

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, case1, sizeof(case1), 7));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), NULL);
	assertEqualString(archive_entry_pathname_utf8(ae), "ABC\xe2\x86\x92.txt");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_zip_utf8_paths_case1_streaming)
{
	struct archive *a;
	struct archive_entry *ae;

	/* Verify with streaming reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, case1, sizeof(case1), 31));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), NULL);
	assertEqualString(archive_entry_pathname_utf8(ae), "ABC\xe2\x86\x92.txt");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_free(a));
}

/*
 * TODO: Case 2: GP#11 is used, but filename is not valid UTF-8.
 * This should always cause an error; malformed UTF-8 should never happen.
 */

/*
 * TODO: Case 3: Store UTF-8 filename using extra field 0x7075
 * 0x7075 filename and regular filename have identical bytes but
 * regular filename is not marked with GP#11 bit.
 *
 * Note: Central dir entry has only "A.txt" and no 0x7075 extension.
 */
static const unsigned char case3[] = {
	/* Local file header */
	0x50, 0x4b, 0x03, 0x04, /* PK\003\004 */
	0x20, 0x00, /* Version needed to extract: 2.0 */
	0x00, 0x00, /* General purpose bit flag: 0x0000 */
	0x00, 0x00, /* Compression method: None */
	0x00, 0x00, /* Last mod time */
	0x00, 0x00, /* Last mod date */
	0x00, 0x00, 0x00, 0x00, /* CRC32 */
	0x04, 0x00, 0x00, 0x00, /* Compressed size: 4 */
	0x04, 0x00, 0x00, 0x00, /* Uncompressed size: 4 */
	0x0a, 0x00, /* Filename length: 10 */
	0x0e, 0x00, /* Extra field length: 14 */
	0x41, 0x42, 0x43, 0xE2, 0x86, 0x92, 0x2e, 0x74, 0x78, 0x74, /* Filename: ABC<right arrow>.txt */
	0x75, 0x70, 0x0a, 0x00, 0x41, 0x42, 0x43, 0xE2, 0x86, 0x92, 0x2e, 0x74, 0x78, 0x74, /* Extra field: 0x7075 */
	
	/* File data */
	0x41, 0x42, 0x43, 0x0a, /* "ABC\n" */

	/* Central directory header */
	0x50, 0x4b, 0x01, 0x02, /* PK\001\002 */
	0x20, 0x00, /* Version made by: 2.0 for MSDOS */
	0x20, 0x00, /* Version needed to extract: 2.0 */
	0x00, 0x08, /* General purpose bit flag: bit 11 = UTF8 filename */
	0x00, 0x00, /* Compression method: None */
	0x00, 0x00, /* Last mod time */
	0x00, 0x00, /* Last mod date */
	0x00, 0x00, 0x00, 0x00, /* CRC32 */
	0x04, 0x00, 0x00, 0x00, /* Compressed size: 4 */
	0x04, 0x00, 0x00, 0x00, /* Uncompressed size: 4 */
	0x05, 0x00, /* Filename length */
	0x00, 0x00, /* Extra field length: 0 */
	0x00, 0x00, /* Comment length: 0 */
	0x00, 0x00, /* Disk number start: 0 */
	0x00, 0x00, /* Internal file attributes */
	0x00, 0x00, 0x00, 0x00, /* External file attributes */
	0x00, 0x00, 0x00, 0x00, /* Offset of local header */
	0x41, 0x2e, 0x74, 0x78, 0x74, /* File name */
	/* No extra fields */
	/* File comment: not present */

	/* End of central directory record */
	0x50, 0x4b, 0x05, 0x06, /* PK\005\006 */
	0x00, 0x00, /* Number of this disk: 0 */
	0x00, 0x00, /* Central directory starts on this disk: 0 */
	0x01, 0x00, /* Total CD entries on this disk: 1 */
	0x01, 0x00, /* Total CD entries: 1 */
	0x33, 0x00, 0x00, 0x00, /* Size of CD in bytes */
	0x3a, 0x00, 0x00, 0x00, /* Offset of start of CD */
	0x00, 0x00, /* Length of archive comment: 0 */
	/* Archive comment: not present */
};

DEFINE_TEST(test_read_format_zip_utf8_paths_case3_seeking)
{
	struct archive *a;
	struct archive_entry *ae;

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, case3, sizeof(case3), 7));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), NULL);
	assertEqualString(archive_entry_pathname_utf8(ae), "ABC\xe2\x86\x92.txt");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_zip_utf8_paths_case3_streaming)
{
	struct archive *a;
	struct archive_entry *ae;

	/* Verify with streaming reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, case3, sizeof(case3), 31));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), NULL);
	assertEqualString(archive_entry_pathname_utf8(ae), "ABC\xe2\x86\x92.txt");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_free(a));
}


/*
 * TODO: Case 4: As with Case 3, but the two filenames are not
 * the same.
 */

/*
 * TODO: Case 5: GP#11 and extra field 0x7075 both used, but
 * store different names.
 */

/*
 * TODO: Similar cases where the local file header and central directory
 * disagree.  Seeking reader should always use the CD version, streaming
 * reader must necessarily always use the local file header version.
 */
