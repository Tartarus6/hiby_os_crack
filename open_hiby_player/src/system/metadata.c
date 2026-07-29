/*
 * This file is mostly LLM written, be warned.
 */

#include "metadata.h"

#include "src/system/decode/decode.h"
#include "src/system/decode/dr_flac.h"
#include "src/system/decode/stb_vorbis_decl.h"
#include "src/system/utils.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Standard ID3v1 genre list, used to resolve numeric genre references from
// both ID3v1 tags and ID3v2 TCON frames written in the old "(N)" style.
static const char *id3v1_genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
	"Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
	"Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
	"Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
	"Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
	"Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
	"AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative",
	"Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
	"Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
	"Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap",
	"Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
	"Psychedelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
	"Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll",
	"Hard Rock",
};
#define ID3V1_GENRE_COUNT (sizeof(id3v1_genres) / sizeof(id3v1_genres[0]))

// Copies a null-terminated string into a fixed-size destination, truncating
// safely. Avoids -Wformat-truncation noise from snprintf("%s", ...) when the
// compiler can't prove the source is short enough.
static void copy_bounded(char *dst, size_t dst_size, const char *src) {
	if (dst_size == 0)
		return;
	size_t len = strlen(src);
	size_t copy_len = len < dst_size - 1 ? len : dst_size - 1;
	memcpy(dst, src, copy_len);
	dst[copy_len] = '\0';
}

// ---------------------------------------------------------------------------
// Vorbis comment parsing, shared by FLAC and OGG Vorbis
// ---------------------------------------------------------------------------

// Applies a single "KEY=VALUE" Vorbis comment string (not null-terminated) to
// the relevant metadata field.
static void apply_vorbis_comment(song_metadata_t *out, const char *comment, size_t len) {
	const char *eq = memchr(comment, '=', len);
	if (!eq)
		return;

	size_t key_len = (size_t)(eq - comment);
	size_t value_len = len - key_len - 1;
	const char *value = eq + 1;

	char key[32];
	if (key_len >= sizeof(key))
		return;
	for (size_t i = 0; i < key_len; i++) {
		key[i] = (char)toupper((unsigned char)comment[i]);
	}
	key[key_len] = '\0';

	char *dst = NULL;
	size_t dst_size = 0;
	if (strcmp(key, "TITLE") == 0) {
		dst = out->title;
		dst_size = sizeof(out->title);
	} else if (strcmp(key, "ARTIST") == 0) {
		dst = out->artist;
		dst_size = sizeof(out->artist);
	} else if (strcmp(key, "ALBUM") == 0) {
		dst = out->album;
		dst_size = sizeof(out->album);
	} else if (strcmp(key, "GENRE") == 0) {
		dst = out->genre;
		dst_size = sizeof(out->genre);
	} else if (strcmp(key, "TRACKNUMBER") == 0) {
		char num_buf[16];
		size_t n = value_len < sizeof(num_buf) - 1 ? value_len : sizeof(num_buf) - 1;
		memcpy(num_buf, value, n);
		num_buf[n] = '\0';
		out->track_number = (int)strtol(num_buf, NULL, 10);
		return;
	} else if (strcmp(key, "DATE") == 0) {
		char num_buf[16];
		size_t n = value_len < sizeof(num_buf) - 1 ? value_len : sizeof(num_buf) - 1;
		memcpy(num_buf, value, n);
		num_buf[n] = '\0';
		out->year = (int)strtol(num_buf, NULL, 10);
		return;
	} else {
		return;
	}

	size_t copy_len = value_len < dst_size - 1 ? value_len : dst_size - 1;
	memcpy(dst, value, copy_len);
	dst[copy_len] = '\0';
}

static void flac_meta_callback(void *pUserData, drflac_metadata *pMetadata) {
	if (pMetadata->type != DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
		return;

	song_metadata_t *out = (song_metadata_t *)pUserData;

	drflac_vorbis_comment_iterator iter;
	drflac_init_vorbis_comment_iterator(&iter, pMetadata->data.vorbis_comment.commentCount,
										 pMetadata->data.vorbis_comment.pComments);

	drflac_uint32 comment_len;
	const char *comment;
	while ((comment = drflac_next_vorbis_comment(&iter, &comment_len)) != NULL) {
		apply_vorbis_comment(out, comment, comment_len);
	}
}

static void read_flac_metadata(const char *filepath, song_metadata_t *out) {
	drflac *flac = drflac_open_file_with_metadata(filepath, flac_meta_callback, out, NULL);
	if (flac) {
		drflac_close(flac);
	}
}

static void read_ogg_metadata(const char *filepath, song_metadata_t *out) {
	int error = 0;
	stb_vorbis *vorbis = stb_vorbis_open_filename(filepath, &error, NULL);
	if (!vorbis)
		return;

	stb_vorbis_comment comment = stb_vorbis_get_comment(vorbis);
	for (int i = 0; i < comment.comment_list_length; i++) {
		apply_vorbis_comment(out, comment.comment_list[i], strlen(comment.comment_list[i]));
	}

	stb_vorbis_close(vorbis);
}

// ---------------------------------------------------------------------------
// MP3 (ID3v1 / ID3v2) parsing
// ---------------------------------------------------------------------------

static uint32_t syncsafe_to_uint32(const uint8_t b[4]) {
	return ((uint32_t)(b[0] & 0x7F) << 21) | ((uint32_t)(b[1] & 0x7F) << 14) |
		   ((uint32_t)(b[2] & 0x7F) << 7) | (uint32_t)(b[3] & 0x7F);
}

static uint32_t plain_to_uint32(const uint8_t b[4]) {
	return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

// Converts UTF-16 (LE or BE) text to UTF-8, stopping at a null terminator.
static void utf16_to_utf8(const uint8_t *data, size_t len, bool big_endian, char *out, size_t out_size) {
	size_t oi = 0;
	size_t i = 0;
	while (i + 1 < len && oi + 4 < out_size) {
		uint32_t unit = big_endian ? ((uint32_t)data[i] << 8 | data[i + 1]) : ((uint32_t)data[i + 1] << 8 | data[i]);
		i += 2;
		if (unit == 0)
			break;

		uint32_t cp = unit;
		if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < len) {
			uint32_t unit2 = big_endian ? ((uint32_t)data[i] << 8 | data[i + 1]) : ((uint32_t)data[i + 1] << 8 | data[i]);
			if (unit2 >= 0xDC00 && unit2 <= 0xDFFF) {
				cp = 0x10000 + ((unit - 0xD800) << 10) + (unit2 - 0xDC00);
				i += 2;
			}
		}

		if (cp < 0x80) {
			out[oi++] = (char)cp;
		} else if (cp < 0x800) {
			out[oi++] = (char)(0xC0 | (cp >> 6));
			out[oi++] = (char)(0x80 | (cp & 0x3F));
		} else if (cp < 0x10000) {
			out[oi++] = (char)(0xE0 | (cp >> 12));
			out[oi++] = (char)(0x80 | ((cp >> 6) & 0x3F));
			out[oi++] = (char)(0x80 | (cp & 0x3F));
		} else {
			out[oi++] = (char)(0xF0 | (cp >> 18));
			out[oi++] = (char)(0x80 | ((cp >> 12) & 0x3F));
			out[oi++] = (char)(0x80 | ((cp >> 6) & 0x3F));
			out[oi++] = (char)(0x80 | (cp & 0x3F));
		}
	}
	out[oi] = '\0';
}

// Decodes ID3v2 frame text (encoding byte 0x00-0x03) into UTF-8.
static void id3_decode_text(uint8_t encoding, const uint8_t *data, size_t len, char *out, size_t out_size) {
	if (out_size == 0)
		return;
	out[0] = '\0';
	if (len == 0)
		return;

	switch (encoding) {
	case 0x00: { // ISO-8859-1 (Latin-1)
		size_t oi = 0;
		for (size_t i = 0; i < len && oi + 2 < out_size; i++) {
			uint8_t c = data[i];
			if (c == 0)
				break;
			if (c < 0x80) {
				out[oi++] = (char)c;
			} else {
				out[oi++] = (char)(0xC0 | (c >> 6));
				out[oi++] = (char)(0x80 | (c & 0x3F));
			}
		}
		out[oi] = '\0';
		break;
	}
	case 0x01: { // UTF-16 with BOM
		bool big_endian = false;
		size_t offset = 0;
		if (len >= 2) {
			if (data[0] == 0xFE && data[1] == 0xFF) {
				big_endian = true;
				offset = 2;
			} else if (data[0] == 0xFF && data[1] == 0xFE) {
				big_endian = false;
				offset = 2;
			}
		}
		utf16_to_utf8(data + offset, len - offset, big_endian, out, out_size);
		break;
	}
	case 0x02: // UTF-16BE, no BOM
		utf16_to_utf8(data, len, true, out, out_size);
		break;
	case 0x03: // UTF-8
	default: {
		size_t copy_len = len < out_size - 1 ? len : out_size - 1;
		size_t actual = 0;
		while (actual < copy_len && data[actual] != 0)
			actual++;
		memcpy(out, data, actual);
		out[actual] = '\0';
		break;
	}
	}
}

// ID3v2 TCON content is either plain text, or the old-style "(N)" / "(N)Text"
// format referencing an ID3v1 genre index.
static void resolve_tcon_genre(const char *raw, char *out, size_t out_size) {
	if (raw[0] == '(') {
		const char *end = strchr(raw, ')');
		if (end) {
			char num_buf[8];
			size_t num_len = (size_t)(end - raw - 1);
			if (num_len > 0 && num_len < sizeof(num_buf)) {
				memcpy(num_buf, raw + 1, num_len);
				num_buf[num_len] = '\0';

				char *endptr;
				long idx = strtol(num_buf, &endptr, 10);
				if (*endptr == '\0' && idx >= 0) {
					const char *rest = end + 1;
					if (*rest != '\0') {
						copy_bounded(out, out_size, rest);
						return;
					}
					if ((size_t)idx < ID3V1_GENRE_COUNT) {
						copy_bounded(out, out_size, id3v1_genres[idx]);
						return;
					}
				}
			}
		}
	}
	copy_bounded(out, out_size, raw);
}

// Reads an ID3v2 tag at the start of the file, if present. Returns true if a
// tag was found (regardless of whether any wanted frames were in it).
static bool read_id3v2(FILE *f, song_metadata_t *out) {
	uint8_t header[10];
	if (fread(header, 1, 10, f) != 10)
		return false;
	if (memcmp(header, "ID3", 3) != 0)
		return false;

	uint8_t major_version = header[3];
	uint8_t flags = header[5];
	uint32_t tag_size = syncsafe_to_uint32(&header[6]);

	long tag_data_start = ftell(f);
	long tag_end = tag_data_start + (long)tag_size;

	if (flags & 0x40) { // extended header present
		uint8_t ext_size_bytes[4];
		if (fread(ext_size_bytes, 1, 4, f) != 4)
			return true;
		uint32_t ext_size = (major_version >= 4) ? syncsafe_to_uint32(ext_size_bytes) : plain_to_uint32(ext_size_bytes);
		long remaining = (major_version >= 4) ? (long)ext_size - 4 : (long)ext_size;
		if (remaining > 0)
			fseek(f, remaining, SEEK_CUR);
	}

	while (ftell(f) < tag_end - 10) {
		uint8_t frame_header[10];
		if (fread(frame_header, 1, 10, f) != 10)
			break;
		if (frame_header[0] == 0) // padding reached
			break;

		char frame_id[5] = {0};
		memcpy(frame_id, frame_header, 4);
		uint32_t frame_size = (major_version >= 4) ? syncsafe_to_uint32(&frame_header[4]) : plain_to_uint32(&frame_header[4]);

		if (frame_size == 0 || (long)frame_size > tag_end - ftell(f))
			break;

		bool wanted = strcmp(frame_id, "TIT2") == 0 || strcmp(frame_id, "TPE1") == 0 ||
					  strcmp(frame_id, "TALB") == 0 || strcmp(frame_id, "TCON") == 0 ||
					  strcmp(frame_id, "TRCK") == 0 || strcmp(frame_id, "TYER") == 0 ||
					  strcmp(frame_id, "TDRC") == 0;

		if (!wanted) {
			fseek(f, (long)frame_size, SEEK_CUR);
			continue;
		}

		uint8_t *buf = malloc(frame_size);
		if (!buf)
			break;
		if (fread(buf, 1, frame_size, f) != frame_size) {
			free(buf);
			break;
		}

		uint8_t encoding = buf[0];
		char decoded[512];
		id3_decode_text(encoding, buf + 1, frame_size - 1, decoded, sizeof(decoded));
		free(buf);

		if (strcmp(frame_id, "TIT2") == 0) {
			copy_bounded(out->title, sizeof(out->title), decoded);
		} else if (strcmp(frame_id, "TPE1") == 0) {
			copy_bounded(out->artist, sizeof(out->artist), decoded);
		} else if (strcmp(frame_id, "TALB") == 0) {
			copy_bounded(out->album, sizeof(out->album), decoded);
		} else if (strcmp(frame_id, "TCON") == 0) {
			resolve_tcon_genre(decoded, out->genre, sizeof(out->genre));
		} else if (strcmp(frame_id, "TRCK") == 0) {
			out->track_number = (int)strtol(decoded, NULL, 10);
		} else if (strcmp(frame_id, "TYER") == 0 || strcmp(frame_id, "TDRC") == 0) {
			if (out->year == 0)
				out->year = (int)strtol(decoded, NULL, 10);
		}
	}

	return true;
}

// Copies up to `max_src_len` raw ISO-8859-1 bytes from `src`, trimming
// trailing spaces, and decodes them into `dst` as UTF-8.
static void set_field_bounded(char *dst, size_t dst_size, const char *src, size_t max_src_len) {
	size_t len = 0;
	while (len < max_src_len && src[len] != '\0')
		len++;
	while (len > 0 && src[len - 1] == ' ')
		len--;
	id3_decode_text(0x00, (const uint8_t *)src, len, dst, dst_size);
}

// Reads the trailing 128-byte ID3v1 tag, filling only fields left empty by
// a preceding ID3v2 pass (if any).
static void read_id3v1(FILE *f, song_metadata_t *out) {
	if (fseek(f, -128, SEEK_END) != 0)
		return;

	uint8_t tag[128];
	if (fread(tag, 1, 128, f) != 128)
		return;
	if (memcmp(tag, "TAG", 3) != 0)
		return;

	if (out->title[0] == '\0')
		set_field_bounded(out->title, sizeof(out->title), (const char *)tag + 3, 30);
	if (out->artist[0] == '\0')
		set_field_bounded(out->artist, sizeof(out->artist), (const char *)tag + 33, 30);
	if (out->album[0] == '\0')
		set_field_bounded(out->album, sizeof(out->album), (const char *)tag + 63, 30);

	if (out->year == 0) {
		char year_buf[5] = {0};
		memcpy(year_buf, tag + 93, 4);
		out->year = (int)strtol(year_buf, NULL, 10);
	}

	if (out->genre[0] == '\0') {
		uint8_t genre_idx = tag[127];
		if (genre_idx < ID3V1_GENRE_COUNT) {
			snprintf(out->genre, sizeof(out->genre), "%s", id3v1_genres[genre_idx]);
		}
	}
}

static void read_mp3_metadata(const char *filepath, song_metadata_t *out) {
	FILE *f = fopen(filepath, "rb");
	if (!f)
		return;

	read_id3v2(f, out);
	read_id3v1(f, out);

	fclose(f);
}

// ---------------------------------------------------------------------------
// WAV (RIFF LIST/INFO chunk) parsing
// ---------------------------------------------------------------------------

static void read_wav_metadata(const char *filepath, song_metadata_t *out) {
	FILE *f = fopen(filepath, "rb");
	if (!f)
		return;

	char riff_header[12];
	if (fread(riff_header, 1, 12, f) != 12 || memcmp(riff_header, "RIFF", 4) != 0 ||
		memcmp(riff_header + 8, "WAVE", 4) != 0) {
		fclose(f);
		return;
	}

	struct {
		char id[4];
		uint32_t size;
	} chunk;

	while (fread(&chunk, 1, sizeof(chunk), f) == sizeof(chunk)) {
		long chunk_data_start = ftell(f);

		if (memcmp(chunk.id, "LIST", 4) == 0 && chunk.size >= 4) {
			char list_type[4];
			if (fread(list_type, 1, 4, f) == 4 && memcmp(list_type, "INFO", 4) == 0) {
				long list_end = chunk_data_start + (long)chunk.size;

				struct {
					char id[4];
					uint32_t size;
				} sub;
				while (ftell(f) + (long)sizeof(sub) <= list_end && fread(&sub, 1, sizeof(sub), f) == sizeof(sub)) {
					char value[1024];
					uint32_t read_size = sub.size < sizeof(value) - 1 ? sub.size : sizeof(value) - 1;
					if (fread(value, 1, read_size, f) != read_size)
						break;
					value[read_size] = '\0';

					char *dst = NULL;
					size_t dst_size = 0;
					if (memcmp(sub.id, "INAM", 4) == 0) {
						dst = out->title;
						dst_size = sizeof(out->title);
					} else if (memcmp(sub.id, "IART", 4) == 0) {
						dst = out->artist;
						dst_size = sizeof(out->artist);
					} else if (memcmp(sub.id, "IPRD", 4) == 0) {
						dst = out->album;
						dst_size = sizeof(out->album);
					} else if (memcmp(sub.id, "IGNR", 4) == 0) {
						dst = out->genre;
						dst_size = sizeof(out->genre);
					}
					if (dst) {
						size_t copy_len = read_size < dst_size - 1 ? read_size : dst_size - 1;
						memcpy(dst, value, copy_len);
						dst[copy_len] = '\0';
					}

					long sub_data_start = ftell(f) - (long)read_size;
					long next_sub_pos = sub_data_start + (long)sub.size + (sub.size & 1);
					fseek(f, next_sub_pos, SEEK_SET);
				}
			}
		}

		long next_chunk_pos = chunk_data_start + (long)chunk.size + (chunk.size & 1);
		fseek(f, next_chunk_pos, SEEK_SET);
	}

	fclose(f);
}

// ---------------------------------------------------------------------------

void metadata_read(const char *filepath, song_metadata_t *out) {
	memset(out, 0, sizeof(*out));

	decode_format_t format = decode_detect_format(filepath);
	switch (format) {
	case DECODE_FORMAT_MP3:
		read_mp3_metadata(filepath, out);
		break;
	case DECODE_FORMAT_FLAC:
		read_flac_metadata(filepath, out);
		break;
	case DECODE_FORMAT_OGG_VORBIS:
		read_ogg_metadata(filepath, out);
		break;
	default:
		if (has_extension(filepath, ".wav")) {
			read_wav_metadata(filepath, out);
		}
		break;
	}

	out->has_tags = out->title[0] != '\0' || out->artist[0] != '\0' || out->album[0] != '\0' || out->genre[0] != '\0';
}
