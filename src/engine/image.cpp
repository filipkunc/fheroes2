/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2020 - 2026                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "image_palette.h"
#include "screen.h"

namespace
{
    // 0 in shadow part means no shadow, 1 means skip any drawings so to don't waste extra CPU cycles for ( tableId - 2 ) command we just add extra fake tables
    // Mirror palette was modified as it was containing 238, 238, 239, 240 values instead of 238, 239, 240, 241
    const uint8_t transformTable[256 * 16] = {
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,
        35,  36,  36,  36,  36,  36,  36,  36,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  62,
        62,  62,  62,  62,  62,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  84,  84,  84,  84,  84,  91,  92,
        93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 107, 107, 107, 107, 107, 107, 114, 115, 116, 117, 118, 119, 120, 121,
        122, 123, 124, 125, 126, 127, 128, 129, 130, 130, 130, 130, 130, 130, 130, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
        150, 151, 151, 151, 151, 151, 151, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 174, 174, 174, 174, 174,
        174, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197, 197, 197, 202, 203, 204, 205, 206,
        207, 208, 209, 210, 211, 212, 213, 213, 213, 213, 213, 214, 215, 216, 217, 218, 219, 220, 221, 225, 226, 227, 228, 229, 230, 230, 230, 230, 73,
        75,  77,  79,  81,  76,  78,  74,  76,  78,  80,  244, 245, 245, 245, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // First

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
        33,  34,  35,  36,  36,  36,  36,  36,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,
        62,  62,  62,  62,  62,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  84,  84,  84,  89,  90,
        91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 107, 107, 107, 107, 112, 113, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 130, 130, 130, 130, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147,
        148, 149, 150, 151, 151, 151, 151, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 174, 174, 174,
        174, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 197, 201, 202, 203, 204, 205,
        206, 207, 208, 209, 210, 211, 212, 213, 213, 213, 213, 214, 215, 216, 217, 218, 219, 220, 221, 224, 225, 226, 227, 228, 229, 230, 230, 230, 76,
        76,  76,  76,  76,  76,  76,  76,  76,  76,  78,  244, 245, 245, 245, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Second

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,
        31,  32,  33,  34,  35,  36,  36,  36,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
        60,  61,  62,  62,  62,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  84,  84,  87,  88,
        89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 107, 107, 110, 111, 112, 113, 114, 115, 116, 117,
        118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 130, 130, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146,
        147, 148, 149, 150, 151, 151, 151, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 174,
        174, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 197, 200, 201, 202, 203, 204,
        205, 206, 207, 208, 209, 210, 211, 212, 213, 213, 213, 214, 215, 216, 217, 218, 219, 220, 221, 223, 224, 225, 226, 227, 228, 229, 230, 230, 76,
        76,  76,  76,  76,  76,  76,  76,  76,  76,  76,  243, 244, 245, 245, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Third

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
        30,  31,  32,  33,  34,  35,  36,  36,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
        59,  60,  61,  62,  62,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  84,  86,  87,
        88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 107, 109, 110, 111, 112, 113, 114, 115, 116,
        117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 130, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
        146, 147, 148, 149, 150, 151, 151, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174,
        174, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 197, 199, 200, 201, 202, 203,
        204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 213, 214, 215, 216, 217, 218, 219, 220, 221, 223, 224, 225, 226, 227, 228, 229, 230, 230, 75,
        75,  75,  75,  75,  75,  75,  75,  75,  75,  75,  243, 244, 245, 245, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Fourth

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  10,  11,  11,  11,  12,  13,  13,  13,  14,  14,  15,  15,  15,  16,  17,  17,  17,  18,
        18,  19,  19,  20,  20,  20,  21,  21,  11,  37,  37,  37,  38,  38,  39,  39,  39,  40,  40,  41,  41,  41,  41,  42,  42,  19,  42,  20,  20,
        20,  20,  20,  20,  21,  12,  131, 63,  63,  63,  64,  64,  64,  65,  65,  65,  65,  65,  242, 242, 242, 242, 242, 242, 242, 242, 242, 13,  14,
        15,  15,  16,  85,  17,  85,  85,  85,  85,  19,  86,  20,  20,  20,  21,  21,  21,  21,  21,  21,  21,  10,  108, 108, 109, 109, 109, 110, 110,
        110, 110, 199, 40,  41,  41,  41,  41,  41,  42,  42,  42,  42,  20,  20,  11,  11,  131, 131, 132, 132, 132, 133, 133, 134, 134, 134, 135, 135,
        18,  136, 19,  19,  20,  20,  20,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  15,  15,  15,  16,  17,  17,  17,  18,  18,  19,  19,  20,
        20,  11,  175, 175, 176, 176, 38,  177, 177, 178, 178, 178, 179, 179, 179, 179, 180, 180, 180, 180, 180, 180, 21,  21,  108, 108, 38,  109, 38,
        109, 39,  40,  40,  41,  41,  41,  42,  42,  42,  20,  199, 179, 180, 180, 110, 110, 40,  42,  110, 110, 86,  86,  86,  86,  18,  18,  19,  65,
        65,  65,  66,  65,  66,  65,  152, 155, 65,  242, 15,  16,  17,  19,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Fifth

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  11,  12,  12,  13,  13,  14,  15,  15,  16,  16,  17,  17,  18,  19,  20,  20,  21,
        21,  22,  22,  23,  24,  24,  25,  25,  37,  37,  38,  38,  39,  39,  40,  41,  41,  41,  42,  42,  43,  43,  44,  44,  45,  45,  46,  46,  23,
        24,  24,  24,  24,  24,  131, 63,  63,  64,  64,  65,  65,  66,  66,  242, 67,  67,  68,  68,  243, 243, 243, 243, 243, 243, 243, 243, 15,  15,
        85,  85,  85,  85,  86,  86,  87,  87,  88,  88,  88,  88,  89,  24,  90,  25,  25,  25,  25,  25,  25,  37,  108, 109, 109, 110, 110, 111, 111,
        200, 200, 201, 201, 42,  43,  43,  44,  44,  44,  45,  45,  46,  46,  46,  11,  131, 132, 132, 132, 133, 133, 134, 135, 135, 136, 242, 137, 137,
        138, 243, 243, 243, 243, 243, 24,  152, 152, 153, 153, 154, 154, 155, 156, 156, 157, 158, 158, 159, 18,  19,  19,  20,  20,  21,  22,  22,  23,
        24,  37,  175, 176, 176, 177, 177, 178, 179, 179, 180, 180, 180, 181, 181, 181, 182, 182, 182, 46,  47,  47,  48,  25,  108, 109, 109, 109, 198,
        199, 199, 201, 201, 42,  43,  43,  44,  45,  46,  46,  201, 181, 182, 183, 111, 111, 202, 45,  111, 111, 87,  88,  88,  88,  88,  21,  22,  66,
        66,  68,  68,  67,  68,  68,  152, 157, 66,  69,  16,  18,  20,  21,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Sixth

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  11,  12,  13,  14,  14,  15,  16,  17,  17,  18,  19,  20,  20,  21,  22,  23,  24,
        24,  25,  26,  26,  27,  28,  29,  29,  37,  37,  38,  39,  40,  40,  41,  42,  42,  43,  44,  44,  45,  46,  46,  47,  47,  48,  48,  49,  50,
        50,  27,  28,  28,  28,  63,  63,  64,  65,  65,  66,  67,  67,  68,  69,  69,  69,  70,  70,  70,  244, 71,  244, 244, 244, 244, 245, 16,  85,
        85,  86,  87,  87,  88,  88,  89,  90,  90,  91,  91,  91,  92,  93,  93,  93,  29,  29,  29,  29,  29,  37,  109, 109, 110, 111, 111, 112, 113,
        112, 112, 203, 203, 203, 44,  45,  46,  47,  47,  47,  48,  48,  49,  50,  131, 131, 132, 133, 133, 134, 135, 136, 136, 137, 137, 139, 139, 139,
        141, 141, 141, 143, 143, 245, 245, 152, 152, 153, 154, 155, 155, 156, 157, 158, 158, 159, 160, 161, 162, 163, 163, 164, 165, 165, 166, 26,  26,
        27,  175, 13,  176, 177, 178, 178, 179, 180, 181, 181, 182, 182, 183, 183, 183, 184, 184, 185, 185, 50,  50,  52,  52,  109, 109, 198, 199, 200,
        201, 201, 202, 202, 44,  45,  46,  47,  48,  48,  49,  204, 205, 185, 185, 112, 112, 204, 47,  112, 113, 88,  89,  91,  92,  93,  93,  25,  66,
        68,  69,  69,  68,  69,  69,  153, 159, 68,  71,  18,  242, 243, 24,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // Seventh

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  12,  13,  13,  14,  15,  16,  17,  17,  19,  19,  20,  21,  22,  23,  24,  24,  26,
        26,  27,  28,  28,  30,  30,  31,  32,  37,  38,  39,  39,  40,  41,  42,  43,  43,  44,  45,  46,  46,  47,  48,  49,  50,  50,  51,  52,  52,
        53,  54,  54,  30,  31,  63,  64,  64,  65,  66,  67,  68,  69,  69,  70,  71,  71,  71,  72,  72,  72,  73,  73,  73,  168, 168, 168, 85,  85,
        86,  87,  88,  88,  89,  90,  91,  91,  92,  93,  93,  94,  95,  95,  96,  96,  96,  31,  32,  32,  32,  108, 109, 198, 110, 111, 112, 113, 113,
        113, 116, 117, 118, 119, 120, 121, 47,  48,  50,  50,  51,  51,  52,  52,  131, 132, 132, 133, 134, 135, 136, 137, 137, 138, 139, 140, 141, 141,
        143, 143, 144, 145, 146, 147, 30,  152, 153, 153, 154, 155, 156, 157, 158, 158, 159, 160, 161, 162, 163, 164, 165, 165, 166, 167, 168, 169, 28,
        29,  175, 176, 177, 177, 178, 179, 180, 181, 182, 182, 183, 184, 185, 185, 185, 186, 186, 187, 50,  52,  52,  54,  55,  109, 198, 199, 200, 201,
        202, 202, 204, 204, 205, 207, 47,  49,  50,  51,  52,  206, 206, 187, 188, 113, 113, 118, 49,  222, 222, 223, 224, 225, 226, 95,  227, 228, 67,
        68,  70,  71,  69,  71,  70,  153, 65,  69,  73,  242, 22,  243, 244, 0,   0,   0,   0,   0,   0,   0,   0,   0,
        0, // Eighth

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  11,  12,  12,  13,  14,  14,  15,  16,  16,  17,  18,  18,  19,  20,  242, 242, 22,
        22,  23,  243, 243, 25,  244, 244, 244, 11,  37,  38,  38,  39,  40,  40,  41,  178, 18,  19,  20,  20,  21,  21,  22,  22,  22,  22,  22,  23,
        23,  23,  23,  24,  244, 63,  63,  64,  64,  65,  65,  66,  66,  67,  67,  68,  68,  68,  69,  69,  69,  69,  69,  69,  70,  70,  70,  15,  15,
        16,  85,  86,  18,  19,  19,  20,  159, 21,  21,  161, 22,  163, 163, 163, 23,  23,  165, 165, 244, 244, 37,  108, 38,  109, 109, 110, 199, 200,
        199, 40,  41,  42,  42,  42,  43,  43,  44,  22,  22,  23,  23,  23,  23,  131, 131, 132, 132, 133, 133, 134, 134, 135, 135, 136, 136, 137, 137,
        138, 138, 139, 139, 140, 141, 244, 152, 152, 153, 153, 154, 154, 155, 155, 156, 156, 157, 158, 158, 159, 242, 159, 161, 161, 243, 243, 243, 243,
        164, 11,  175, 176, 176, 177, 177, 178, 179, 179, 180, 180, 181, 181, 182, 182, 182, 182, 183, 22,  23,  23,  23,  23,  108, 38,  38,  39,  39,
        40,  40,  41,  178, 180, 42,  44,  45,  23,  23,  23,  180, 181, 181, 183, 110, 200, 42,  45,  85,  86,  87,  87,  87,  21,  22,  22,  23,  66,
        66,  67,  68,  67,  68,  68,  153, 158, 67,  70,  64,  65,  242, 243, 159, 159, 159, 159, 159, 159, 159, 159, 159, 10, // Ninth

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  11,  12,  13,  14,  14,  15,  16,  16,  17,  18,  19,  19,  20,  242, 22,  22,  243,
        243, 243, 244, 244, 244, 244, 245, 245, 37,  37,  38,  176, 39,  177, 41,  41,  42,  179, 20,  180, 45,  22,  23,  23,  24,  24,  24,  24,  25,
        25,  25,  25,  26,  26,  63,  63,  64,  64,  65,  66,  67,  67,  68,  68,  69,  69,  70,  70,  70,  70,  70,  71,  71,  71,  71,  71,  15,  85,
        85,  86,  86,  87,  20,  88,  89,  22,  161, 162, 163, 163, 164, 164, 165, 165, 166, 166, 167, 167, 167, 37,  108, 109, 109, 110, 199, 111, 111,
        200, 201, 41,  43,  43,  43,  44,  44,  46,  46,  24,  24,  25,  25,  25,  131, 131, 132, 132, 133, 134, 135, 135, 136, 136, 137, 138, 138, 139,
        139, 140, 140, 141, 141, 142, 143, 152, 152, 153, 153, 154, 155, 155, 156, 156, 157, 158, 158, 159, 159, 161, 161, 162, 162, 163, 164, 244, 244,
        244, 175, 175, 176, 177, 177, 178, 179, 179, 180, 181, 181, 182, 182, 183, 183, 183, 184, 184, 184, 25,  25,  25,  25,  108, 109, 109, 39,  40,
        41,  41,  42,  42,  43,  43,  45,  46,  47,  25,  25,  181, 182, 183, 185, 111, 111, 42,  46,  111, 87,  88,  88,  88,  22,  23,  24,  25,  66,
        67,  68,  69,  68,  69,  69,  153, 0,   68,  71,  65,  242, 242, 243, 0,   0,   0,   0,   0,   0,   0,   0,   0,   10, // Tenth

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  11,  13,  13,  14,  15,  16,  16,  17,  18,  19,  20,  21,  21,  22,  23,  243, 25,
        244, 244, 244, 28,  245, 245, 245, 31,  37,  38,  38,  39,  40,  41,  41,  42,  42,  180, 45,  46,  46,  47,  47,  25,  26,  26,  26,  26,  27,
        27,  27,  27,  27,  245, 63,  63,  64,  65,  66,  66,  67,  68,  69,  69,  70,  70,  71,  71,  72,  72,  72,  72,  72,  73,  73,  73,  16,  85,
        85,  86,  87,  88,  88,  90,  90,  91,  91,  163, 164, 164, 165, 166, 166, 167, 167, 168, 169, 169, 170, 37,  108, 109, 198, 199, 111, 112, 112,
        201, 202, 202, 43,  44,  45,  45,  46,  46,  47,  48,  26,  27,  27,  27,  131, 131, 132, 133, 134, 135, 135, 136, 137, 137, 138, 139, 139, 140,
        141, 141, 142, 143, 143, 144, 145, 152, 152, 153, 154, 155, 155, 156, 156, 158, 158, 159, 160, 160, 161, 162, 163, 164, 164, 165, 166, 166, 167,
        167, 175, 176, 176, 177, 178, 178, 179, 180, 181, 182, 182, 183, 184, 184, 184, 185, 186, 186, 50,  51,  27,  27,  27,  109, 109, 109, 40,  40,
        41,  42,  43,  43,  44,  45,  46,  47,  49,  27,  27,  182, 183, 184, 187, 112, 112, 43,  47,  112, 87,  89,  90,  91,  91,  24,  26,  26,  67,
        68,  69,  70,  69,  70,  70,  153, 0,   0,   73,  65,  242, 243, 244, 0,   0,   0,   0,   0,   0,   0,   0,   0,
        10, // Eleventh

        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   10,  11,  12,  13,  13,  14,  15,  16,  17,  18,  19,  20,  21,  21,  23,  243, 24,  25,  244,
        244, 27,  245, 245, 31,  170, 149, 149, 37,  38,  38,  39,  40,  41,  42,  42,  44,  45,  46,  46,  47,  48,  49,  50,  51,  28,  28,  28,  29,
        29,  29,  29,  29,  30,  63,  64,  65,  65,  66,  67,  68,  69,  70,  70,  71,  72,  72,  73,  73,  74,  74,  75,  75,  76,  76,  76,  85,  85,
        86,  87,  88,  89,  90,  91,  91,  92,  93,  93,  166, 166, 96,  168, 168, 169, 170, 170, 171, 171, 171, 37,  109, 109, 110, 200, 111, 112, 113,
        202, 202, 203, 44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  29,  29,  131, 132, 133, 133, 134, 135, 136, 137, 138, 139, 139, 140, 141, 142,
        142, 144, 144, 145, 145, 146, 147, 152, 153, 153, 154, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 164, 164, 165, 166, 167, 168, 168, 168,
        169, 175, 176, 177, 177, 178, 179, 180, 181, 182, 182, 183, 184, 185, 186, 186, 187, 187, 189, 189, 193, 193, 146, 146, 109, 109, 198, 199, 201,
        201, 201, 44,  205, 45,  46,  47,  48,  50,  52,  29,  183, 185, 186, 189, 112, 112, 205, 49,  222, 88,  89,  91,  92,  93,  26,  27,  28,  67,
        68,  70,  71,  69,  71,  71,  154, 0,   0,   75,  242, 242, 243, 244, 0,   0,   0,   0,   0,   0,   0,   0,   0,   10, // Twelfth

        0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  10,  10,  10,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
        25,  26,  27,  28,  29,  30,  31,  32,  37,  37,  37,  37,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,
        54,  55,  56,  57,  58,  63,  63,  63,  63,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  85,  85,
        85,  85,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 108, 108, 108, 108, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 131, 131, 131, 131, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
        141, 142, 143, 144, 145, 146, 147, 152, 152, 152, 152, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
        170, 175, 175, 175, 175, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 198, 198, 198, 198, 198,
        199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 214, 215, 216, 217, 218, 219, 220, 221, 222, 222, 223, 224, 225, 226, 227, 228, 229, 231,
        232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 242, 243, 244, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, // Mirror

        0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
        29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,
        58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,
        87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
        116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144,
        145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173,
        174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
        203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 188, 188, 188, 188, 118, 118, 118, 118, 222, 223, 224, 225, 226, 227, 228, 229, 230, 69,
        69,  69,  69,  69,  236, 237, 69,  69,  69,  69,  242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 // No cycle
    };

    // Shadow darkening factors for transform values 2-5 when writing to RGBA destinations.
    // Transform 2 = strongest shadow, transform 5 = weakest.
    constexpr float shadowFactor[6] = { 1.0f, 1.0f, 0.25f, 0.40f, 0.55f, 0.70f };

    // Convert a palette index to RGBA bytes (R, G, B, A=255). Uses the currently active
    // render palette (8-bit RGB) — by default the static gamePalette expanded from 6-bit to
    // 8-bit, but Display::changePalette() can point this at e.g. an SMK video's own palette
    // so sprite blits during video playback render with the colours the artwork was authored
    // against (matches the indexed-Display behaviour pre-RGBA refactor).
    inline void paletteIdxToRGBA( const uint8_t idx, uint8_t & r, uint8_t & g, uint8_t & b )
    {
        const uint8_t * pal = fheroes2::getRenderPalette8Bit() + ( static_cast<ptrdiff_t>( idx ) * 3 );
        r = pal[0];
        g = pal[1];
        b = pal[2];
    }

    // Physical-pixel rectangle covered by a single game pixel at (gameX, gameY) when written
    // to a backing buffer with the given physical scale. Floor-on-each-edge tiling keeps
    // adjacent game pixels' blocks contiguous (no gaps) even at fractional scales (e.g. 2.25x);
    // adjacent block sizes can differ by 1 physical pixel. Clamped to [0, bufStride/bufHeight).
    struct PhysicalBlock
    {
        int32_t pXStart;
        int32_t pXEnd;
        int32_t pYStart;
        int32_t pYEnd;
    };

    inline PhysicalBlock toPhysicalBlock( const int32_t gameX, const int32_t gameY, const float scale, const int32_t bufStride, const int32_t bufHeight )
    {
        PhysicalBlock b;
        b.pXStart = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( gameX ) * scale ) );
        b.pXEnd = std::min<int32_t>( bufStride, static_cast<int32_t>( static_cast<float>( gameX + 1 ) * scale ) );
        b.pYStart = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( gameY ) * scale ) );
        b.pYEnd = std::min<int32_t>( bufHeight, static_cast<int32_t>( static_cast<float>( gameY + 1 ) * scale ) );
        return b;
    }

    // Fill a physical-pixel block in an RGBA buffer with a solid (r, g, b, a) value.
    inline void fillRGBABlock( uint8_t * outBase, const PhysicalBlock & pb, const int32_t bufStride, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a )
    {
        for ( int32_t py = pb.pYStart; py < pb.pYEnd; ++py ) {
            uint8_t * dstRow = outBase + ( static_cast<ptrdiff_t>( py ) * bufStride + pb.pXStart ) * 4;
            for ( int32_t px = pb.pXStart; px < pb.pXEnd; ++px, dstRow += 4 ) {
                dstRow[0] = r;
                dstRow[1] = g;
                dstRow[2] = b;
                dstRow[3] = a;
            }
        }
    }

    // Apply a per-pixel shadow-darkening factor to every physical pixel in a block.
    inline void shadeRGBABlock( uint8_t * outBase, const PhysicalBlock & pb, const int32_t bufStride, const float factor )
    {
        for ( int32_t py = pb.pYStart; py < pb.pYEnd; ++py ) {
            uint8_t * dstRow = outBase + ( static_cast<ptrdiff_t>( py ) * bufStride + pb.pXStart ) * 4;
            for ( int32_t px = pb.pXStart; px < pb.pXEnd; ++px, dstRow += 4 ) {
                dstRow[0] = static_cast<uint8_t>( static_cast<float>( dstRow[0] ) * factor );
                dstRow[1] = static_cast<uint8_t>( static_cast<float>( dstRow[1] ) * factor );
                dstRow[2] = static_cast<uint8_t>( static_cast<float>( dstRow[2] ) * factor );
            }
        }
    }

    // Per-pixel src_over alpha blend onto every physical pixel in a block.
    inline void blendRGBABlock( uint8_t * outBase, const PhysicalBlock & pb, const int32_t bufStride, const uint8_t srcR, const uint8_t srcG, const uint8_t srcB,
                                const uint32_t srcA )
    {
        const uint32_t invA = 255 - srcA;
        for ( int32_t py = pb.pYStart; py < pb.pYEnd; ++py ) {
            uint8_t * dstRow = outBase + ( static_cast<ptrdiff_t>( py ) * bufStride + pb.pXStart ) * 4;
            for ( int32_t px = pb.pXStart; px < pb.pXEnd; ++px, dstRow += 4 ) {
                dstRow[0] = static_cast<uint8_t>( ( srcR * srcA + dstRow[0] * invA ) / 255 );
                dstRow[1] = static_cast<uint8_t>( ( srcG * srcA + dstRow[1] * invA ) / 255 );
                dstRow[2] = static_cast<uint8_t>( ( srcB * srcA + dstRow[2] * invA ) / 255 );
                const uint32_t dstA = dstRow[3];
                dstRow[3] = static_cast<uint8_t>( std::min( srcA + ( dstA * invA ) / 255, static_cast<uint32_t>( 255 ) ) );
            }
        }
    }

    bool Validate( const fheroes2::Image & image, const int32_t x, const int32_t y, const int32_t width, const int32_t height )
    {
        if ( image.empty() || width <= 0 || height <= 0 ) {
            // What's the reason to work with empty images?
            return false;
        }

        if ( x < 0 || y < 0 || x + width > image.width() || y + height > image.height() ) {
            return false;
        }

        return true;
    }

    bool Verify( const fheroes2::Image & image, int32_t & x, int32_t & y, int32_t & width, int32_t & height )
    {
        if ( image.empty() || width <= 0 || height <= 0 ) {
            // What's the reason to work with empty images?
            return false;
        }

        const int32_t widthOut = image.width();
        const int32_t heightOut = image.height();

        if ( x < 0 ) {
            const int32_t offsetX = -x;
            if ( offsetX >= width ) {
                return false;
            }

            x = 0;
            width -= offsetX;
        }

        if ( y < 0 ) {
            const int32_t offsetY = -y;
            if ( offsetY >= height ) {
                return false;
            }

            y = 0;
            height -= offsetY;
        }

        if ( x > widthOut || y > heightOut ) {
            return false;
        }

        if ( x + width > widthOut ) {
            const int32_t offsetX = x + width - widthOut;
            if ( offsetX >= width ) {
                return false;
            }
            width -= offsetX;
        }

        if ( y + height > heightOut ) {
            const int32_t offsetY = y + height - heightOut;
            if ( offsetY >= height ) {
                return false;
            }
            height -= offsetY;
        }

        return true;
    }

    bool Verify( int32_t & inX, int32_t & inY, int32_t & outX, int32_t & outY, int32_t & width, int32_t & height, const int32_t widthIn, const int32_t heightIn,
                 const int32_t widthOut, const int32_t heightOut )
    {
        if ( widthIn <= 0 || heightIn <= 0 || widthOut <= 0 || heightOut <= 0 || width <= 0 || height <= 0 ) {
            // What's the reason to work with empty images?
            return false;
        }

        if ( inX < 0 || inY < 0 || inX > widthIn || inY > heightIn ) {
            return false;
        }

        if ( outX < 0 ) {
            const int32_t offsetX = -outX;
            if ( offsetX >= width ) {
                return false;
            }

            inX += offsetX;
            outX = 0;
            width -= offsetX;
        }

        if ( outY < 0 ) {
            const int32_t offsetY = -outY;
            if ( offsetY >= height ) {
                return false;
            }

            inY += offsetY;
            outY = 0;
            height -= offsetY;
        }

        if ( outX > widthOut || outY > heightOut ) {
            return false;
        }

        if ( inX + width > widthIn ) {
            const int32_t offsetX = inX + width - widthIn;
            if ( offsetX >= width ) {
                return false;
            }
            width -= offsetX;
        }

        if ( inY + height > heightIn ) {
            const int32_t offsetY = inY + height - heightIn;
            if ( offsetY >= height ) {
                return false;
            }
            height -= offsetY;
        }

        if ( outX + width > widthOut ) {
            const int32_t offsetX = outX + width - widthOut;
            if ( offsetX >= width ) {
                return false;
            }
            width -= offsetX;
        }

        if ( outY + height > heightOut ) {
            const int32_t offsetY = outY + height - heightOut;
            if ( offsetY >= height ) {
                return false;
            }
            height -= offsetY;
        }

        return true;
    }

    bool Verify( const fheroes2::Image & in, int32_t & inX, int32_t & inY, const fheroes2::Image & out, int32_t & outX, int32_t & outY, int32_t & width,
                 int32_t & height )
    {
        return Verify( inX, inY, outX, outY, width, height, in.width(), in.height(), out.width(), out.height() );
    }

    uint8_t GetPALColorId( const uint8_t red, const uint8_t green, const uint8_t blue )
    {
        static uint8_t rgbToId[64 * 64 * 64];
        static bool isInitialized = false;
        if ( !isInitialized ) {
            isInitialized = true;
            const uint32_t size = 64 * 64 * 64;

            int32_t r = 0;
            int32_t g = 0;
            int32_t b = 0;

            const uint8_t * gamePalette = fheroes2::getGamePalette();

            for ( uint32_t id = 0; id < size; ++id ) {
                r = static_cast<int32_t>( id % 64 );
                g = static_cast<int32_t>( id >> 6 ) % 64;
                b = static_cast<int32_t>( id >> 12 );
                int32_t minDistance = INT32_MAX;
                uint32_t bestPos = 0;

                // Use the "No cycle" palette.
                // The first 10 and the last 10 colors are undefined in the original palette. We skip them to avoid usage of these colors.
                constexpr uint32_t startOffset = 10;
                const uint8_t * correctorX = transformTable + 256 * 15 + startOffset;

                for ( uint32_t i = startOffset; i < 246; ++i, ++correctorX ) {
                    const uint8_t * palette = gamePalette + static_cast<ptrdiff_t>( *correctorX ) * 3;

                    const int32_t sumRed = static_cast<int32_t>( *palette ) + r;
                    const int32_t offsetRed = static_cast<int32_t>( *palette ) - r;
                    ++palette;
                    const int32_t offsetGreen = static_cast<int32_t>( *palette ) - g;
                    ++palette;
                    const int32_t offsetBlue = static_cast<int32_t>( *palette ) - b;
                    ++palette;
                    // Based on "Redmean" color distance calculation (https://www.compuphase.com/cmetric.htm).
                    const int32_t distance = ( 2 * 2 * 256 + sumRed ) * offsetRed * offsetRed + 4 * 2 * 256 * offsetGreen * offsetGreen
                                             + ( 2 * ( 2 * 256 + 255 ) - sumRed ) * offsetBlue * offsetBlue;
                    if ( minDistance > distance ) {
                        minDistance = distance;
                        bestPos = *correctorX;
                    }
                }

                rgbToId[id] = static_cast<uint8_t>( bestPos ); // it's safe to cast
            }
        }

        return rgbToId[red + green * 64 + blue * 64 * 64];
    }

    // Phase 3: materialize the indexed channel into the RGBA buffer for a game-coord ROI,
    // so an in-place RGBA modification (alpha blend, dim) reads the correct underlying
    // colour rather than the stale RGBA that lived under indexed-only pixels. After this
    // runs, every game pixel where mask was set has palette[indexed] resident in the RGBA
    // block, and the mask is cleared so the shader resolves through RGBA going forward.
    //
    // No-op when the image has no indexed channel (everything but Display).
    inline void materializeIndexedRoi( fheroes2::Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height )
    {
        uint8_t * idx = out.indexedBuffer();
        uint8_t * mask = out.maskBuffer();
        if ( idx == nullptr || mask == nullptr ) {
            return;
        }
        const int32_t fbW = out.width();
        const int32_t fbH = out.height();
        if ( outX < 0 ) {
            width += outX;
            outX = 0;
        }
        if ( outY < 0 ) {
            height += outY;
            outY = 0;
        }
        if ( width <= 0 || height <= 0 || outX >= fbW || outY >= fbH ) {
            return;
        }
        if ( outX + width > fbW ) {
            width = fbW - outX;
        }
        if ( outY + height > fbH ) {
            height = fbH - outY;
        }
        const int32_t idxStride = out.indexedStride();
        const float scale = out.physicalScale();
        const int32_t bufStride = out.bufferStride();
        const int32_t bufHeight = out.bufferHeight();
        uint8_t * outBase = out.image();
        bool anyChange = false;
        for ( int32_t row = 0; row < height; ++row ) {
            const ptrdiff_t rowOff = static_cast<ptrdiff_t>( outY + row ) * idxStride + outX;
            for ( int32_t col = 0; col < width; ++col ) {
                const ptrdiff_t cellOff = rowOff + col;
                if ( mask[cellOff] == 0 ) {
                    continue;
                }
                // Indexed pixel — palette LUT to RGBA, clear the mask.
                uint8_t r;
                uint8_t g;
                uint8_t b;
                paletteIdxToRGBA( idx[cellOff], r, g, b );
                const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                mask[cellOff] = 0;
                idx[cellOff] = 0;
                anyChange = true;
                // RGBA pixels carrying a shadow flag (mask=0, idx in [2..5]) deliberately
                // pass through unchanged: the GPU shader will multiply the freshly painted
                // RGBA by shadowFactor[idx] at sample time. Doing the multiply here would
                // bake the factor into the buffer, and any subsequent in-place alpha or
                // shadow primitive would re-read the already-dimmed value and compound.
            }
        }
        if ( anyChange ) {
            out.markIndexedDirty( { outX, outY, width, height } );
        }
    }

    // Phase 3: clear the mask channel for a game-coord ROI on the Display so the GPU
    // composite shader resolves through the freshly painted RGBA pixels (mask == 0 →
    // use RGBA). The indexed buffer doesn't need to be cleared too — the shader looks
    // at the mask first — but we zero it anyway to keep the dual buffers in sync, which
    // simplifies later debugging / cycling logic. No-op on images that don't expose a
    // mask buffer (everything except Display). File-local linkage.
    inline void clearIndexedBboxOnDisplay( fheroes2::Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height )
    {
        uint8_t * idx = out.indexedBuffer();
        uint8_t * mask = out.maskBuffer();
        if ( idx == nullptr || mask == nullptr ) {
            return;
        }
        const int32_t fbW = out.width();
        const int32_t fbH = out.height();
        if ( outX < 0 ) {
            width += outX;
            outX = 0;
        }
        if ( outY < 0 ) {
            height += outY;
            outY = 0;
        }
        if ( width <= 0 || height <= 0 || outX >= fbW || outY >= fbH ) {
            return;
        }
        if ( outX + width > fbW ) {
            width = fbW - outX;
        }
        if ( outY + height > fbH ) {
            height = fbH - outY;
        }
        const int32_t stride = out.indexedStride();
        for ( int32_t row = 0; row < height; ++row ) {
            const ptrdiff_t off = static_cast<ptrdiff_t>( outY + row ) * stride + outX;
            std::memset( idx + off, 0, static_cast<size_t>( width ) );
            std::memset( mask + off, 0, static_cast<size_t>( width ) );
        }
        out.markIndexedDirty( { outX, outY, width, height } );
    }

    void ApplyRawPalette( const fheroes2::Image & in, int32_t inX, int32_t inY, fheroes2::Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                          const uint8_t * palette )
    {
        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();

        // RGBA out + indexed in: read src indexed pixel, remap via palette[]. Phase 3
        // Display fast path writes 1 byte per game pixel to the indexed buffer + 255 to
        // the mask buffer; the GPU shader resolves valid mask cells via palette[] at
        // sample time, so the CPU skips per-game-pixel scale² block expansion + the
        // paletteIdxToRGBA call.
        if ( out.format() == fheroes2::ImageFormat::RGBA_32BIT && in.format() == fheroes2::ImageFormat::INDEXED_8BIT ) {
            const uint8_t * imageInY = in.image() + static_cast<ptrdiff_t>( inY ) * widthIn + inX;
            const uint8_t * transformInY = in.singleLayer() ? nullptr : ( in.transform() + static_cast<ptrdiff_t>( inY ) * widthIn + inX );

            uint8_t * idxBase = out.indexedBuffer();
            uint8_t * maskBase = out.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = out.indexedStride();
                for ( int32_t y = 0; y < height; ++y ) {
                    const uint8_t * imgIn = imageInY;
                    const uint8_t * trIn = transformInY;
                    const ptrdiff_t rowOff = static_cast<ptrdiff_t>( outY + y ) * idxStride + outX;
                    uint8_t * idxOut = idxBase + rowOff;
                    uint8_t * maskOut = maskBase + rowOff;
                    for ( int32_t x = 0; x < width; ++x, ++imgIn, ++idxOut, ++maskOut ) {
                        if ( trIn && *trIn != 0 ) {
                            ++trIn;
                            continue;
                        }
                        *idxOut = palette[*imgIn];
                        *maskOut = 255;
                        if ( trIn ) {
                            ++trIn;
                        }
                    }
                    imageInY += widthIn;
                    if ( transformInY ) {
                        transformInY += widthIn;
                    }
                }
                out.markIndexedDirty( { outX, outY, width, height } );
                return;
            }

            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();

            for ( int32_t y = 0; y < height; ++y ) {
                const uint8_t * imgIn = imageInY;
                const uint8_t * trIn = transformInY;

                for ( int32_t x = 0; x < width; ++x, ++imgIn ) {
                    if ( trIn && *trIn != 0 ) {
                        ++trIn;
                        continue;
                    }

                    uint8_t r;
                    uint8_t g;
                    uint8_t b;
                    paletteIdxToRGBA( palette[*imgIn], r, g, b );
                    const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                    fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                    if ( trIn ) {
                        ++trIn;
                    }
                }

                imageInY += widthIn;
                if ( transformInY ) {
                    transformInY += widthIn;
                }
            }
            return;
        }

        // RGBA in + RGBA out: remap each visible pixel's palette index through palette[].
        // Used by ApplyPalette/ApplyAlpha on Display (or any RGBA buffer).
        //
        // Phase 3 subtlety: on Display the visible pixel for mask=255 cells lives in the
        // indexed channel, not RGBA — palette draws (Copy/Blit/Fill) write 1 byte per game
        // pixel + mask=255 and skip RGBA. Reading from in.image() RGBA for those cells gets
        // stale/garbage data. So when the indexed/mask buffers exist, read through them
        // and write the remapped result straight back to indexed (since palette[srcIdx]
        // is itself a palette index — no need to expand to RGBA). This is what fixed the
        // turn-order grayed-out moved-unit slot showing as solid gray (the GRAY palette +
        // shadow transform combo was reading uninitialised RGBA underneath).
        if ( in.format() == fheroes2::ImageFormat::RGBA_32BIT && out.format() == fheroes2::ImageFormat::RGBA_32BIT ) {
            uint8_t * outIdxBase = out.indexedBuffer();
            uint8_t * outMaskBase = out.maskBuffer();
            const uint8_t * inIdxBase = in.indexedBuffer();
            const uint8_t * inMaskBase = in.maskBuffer();
            const bool useIndexedFastPath = ( outIdxBase != nullptr && outMaskBase != nullptr && inIdxBase != nullptr && inMaskBase != nullptr );

            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * inBase = in.image();
            const int32_t inIdxStride = useIndexedFastPath ? in.indexedStride() : 0;
            const int32_t outIdxStride = useIndexedFastPath ? out.indexedStride() : 0;

            for ( int32_t y = 0; y < height; ++y ) {
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( inY + y ) * inScale );
                const uint8_t * srcRow = inBase + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;
                for ( int32_t x = 0; x < width; ++x ) {
                    uint8_t srcIdx = 0;
                    bool haveIndexed = false;
                    if ( useIndexedFastPath ) {
                        const ptrdiff_t inOff = static_cast<ptrdiff_t>( inY + y ) * inIdxStride + ( inX + x );
                        if ( inMaskBase[inOff] != 0 ) {
                            srcIdx = inIdxBase[inOff];
                            haveIndexed = true;
                        }
                    }
                    if ( !haveIndexed ) {
                        const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( inX + x ) * inScale );
                        const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                        if ( srcPx[3] == 0 ) {
                            // Transparent. Still need to clear the corresponding out indexed/mask
                            // cell if we're on the fast path so a stale entry doesn't leak through.
                            if ( useIndexedFastPath ) {
                                const ptrdiff_t outOff = static_cast<ptrdiff_t>( outY + y ) * outIdxStride + ( outX + x );
                                outMaskBase[outOff] = 0;
                                outIdxBase[outOff] = 0;
                            }
                            continue;
                        }
                        srcIdx = GetPALColorId( srcPx[0] >> 2, srcPx[1] >> 2, srcPx[2] >> 2 );
                    }
                    const uint8_t resultIdx = palette[srcIdx];
                    if ( useIndexedFastPath ) {
                        const ptrdiff_t outOff = static_cast<ptrdiff_t>( outY + y ) * outIdxStride + ( outX + x );
                        outIdxBase[outOff] = resultIdx;
                        outMaskBase[outOff] = 255;
                    }
                    else {
                        uint8_t r;
                        uint8_t g;
                        uint8_t b;
                        paletteIdxToRGBA( resultIdx, r, g, b );
                        const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                        fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                    }
                }
            }
            if ( useIndexedFastPath ) {
                out.markIndexedDirty( { outX, outY, width, height } );
            }
            return;
        }

        const uint8_t * imageInY = in.image() + static_cast<ptrdiff_t>( inY ) * widthIn + inX;
        uint8_t * imageOutY = out.image() + static_cast<ptrdiff_t>( outY ) * widthOut + outX;
        const uint8_t * imageInYEnd = imageInY + static_cast<ptrdiff_t>( height ) * widthIn;

        if ( in.singleLayer() ) {
            for ( ; imageInY != imageInYEnd; imageInY += widthIn, imageOutY += widthOut ) {
                const uint8_t * imageInX = imageInY;
                uint8_t * imageOutX = imageOutY;
                const uint8_t * imageInXEnd = imageInX + width;

                for ( ; imageInX != imageInXEnd; ++imageInX, ++imageOutX ) {
                    *imageOutX = palette[*imageInX];
                }
            }
        }
        else {
            const uint8_t * transformInY = in.transform() + static_cast<ptrdiff_t>( inY ) * widthIn + inX;

            for ( ; imageInY != imageInYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut ) {
                const uint8_t * imageInX = imageInY;
                const uint8_t * transformInX = transformInY;
                uint8_t * imageOutX = imageOutY;
                const uint8_t * imageInXEnd = imageInX + width;

                for ( ; imageInX != imageInXEnd; ++imageInX, ++imageOutX, ++transformInX ) {
                    if ( *transformInX == 0 ) { // only modify pixels with data
                        *imageOutX = palette[*imageInX];
                    }
                }
            }
        }
    }
}

namespace fheroes2
{
    Image::Image( Image && image ) noexcept
        : _data( std::move( image._data ) )
    {
        std::swap( _width, image._width );
        std::swap( _height, image._height );
        std::swap( _singleLayer, image._singleLayer );
        std::swap( _format, image._format );
    }

    Image & Image::operator=( const Image & image )
    {
        if ( this == &image ) {
            return *this;
        }

        copy( image );

        return *this;
    }

    Image & Image::operator=( Image && image ) noexcept
    {
        if ( this == &image ) {
            return *this;
        }

        std::swap( _width, image._width );
        std::swap( _height, image._height );
        std::swap( _data, image._data );
        std::swap( _singleLayer, image._singleLayer );
        std::swap( _format, image._format );

        return *this;
    }

    uint8_t * Image::image()
    {
        return _data.get();
    }

    const uint8_t * Image::image() const
    {
        return _data.get();
    }

    void Image::clear()
    {
        _data.reset();

        _width = 0;
        _height = 0;
    }

    void Image::fill( const uint8_t value )
    {
        if ( empty() ) {
            return;
        }

        if ( _format == ImageFormat::RGBA_32BIT ) {
            // Fill the ENTIRE backing buffer (use bufferStride/bufferHeight so Display fills
            // its full physical-pixel buffer, not just the game-pixel sub-rect).
            const size_t totalSize = static_cast<size_t>( bufferStride() ) * bufferHeight();
            // Treat 'value' as a palette index. Convert and fill RGBA bytes.
            uint8_t r;
            uint8_t g;
            uint8_t b;
            paletteIdxToRGBA( value, r, g, b );
            uint8_t * px = image();
            for ( size_t i = 0; i < totalSize; ++i, px += 4 ) {
                px[0] = r;
                px[1] = g;
                px[2] = b;
                px[3] = 255;
            }
            return;
        }

        const size_t totalSize = static_cast<size_t>( _width ) * _height;

        memset( image(), value, totalSize );

        if ( !_singleLayer ) {
            memset( transform(), static_cast<uint8_t>( 0 ), totalSize );
        }
    }

    void Image::resize( const int32_t width_, const int32_t height_ )
    {
        if ( width_ == _width && height_ == _height ) {
            return;
        }

        if ( width_ <= 0 || height_ <= 0 ) {
            clear();

            return;
        }

        const size_t size = static_cast<size_t>( width_ ) * height_;

        if ( _format == ImageFormat::RGBA_32BIT ) {
            _data.reset( new uint8_t[size * 4] );
        }
        else if ( _singleLayer ) {
            _data.reset( new uint8_t[size] );
        }
        else {
            _data.reset( new uint8_t[size * 2] );
        }

        _width = width_;
        _height = height_;
    }

    void Image::reset()
    {
        if ( !empty() ) {
            if ( _format == ImageFormat::RGBA_32BIT ) {
                // Zero the ENTIRE backing buffer (Display has a physical-resolution buffer
                // larger than _width * _height — use bufferStride/bufferHeight virtuals to
                // pick up the actual byte count).
                const size_t totalBytes = static_cast<size_t>( bufferStride() ) * bufferHeight() * 4;
                memset( image(), 0, totalBytes );
            }
            else {
                const size_t totalSize = static_cast<size_t>( _width ) * _height;
                memset( image(), static_cast<uint8_t>( 0 ), totalSize );

                if ( !_singleLayer ) {
                    // Set the transform layer to skip all data.
                    memset( transform(), static_cast<uint8_t>( 1 ), totalSize );
                }
            }
        }
    }

    void Image::copy( const Image & image )
    {
        if ( !image._data ) {
            clear();

            return;
        }

        const size_t imageSize = static_cast<size_t>( image._width ) * image._height;

        _singleLayer = image._singleLayer;
        _format = image._format;

        const bool needsRealloc = ( image._width != _width || image._height != _height );

        if ( needsRealloc ) {
            if ( _format == ImageFormat::RGBA_32BIT ) {
                _data.reset( new uint8_t[imageSize * 4] );
            }
            else if ( _singleLayer ) {
                _data.reset( new uint8_t[imageSize] );
            }
            else {
                _data.reset( new uint8_t[imageSize * 2] );
            }

            _width = image._width;
            _height = image._height;
        }

        size_t copySize = imageSize;
        if ( _format == ImageFormat::RGBA_32BIT ) {
            copySize = imageSize * 4;
        }
        else if ( !_singleLayer ) {
            copySize = imageSize * 2;
        }

        memcpy( _data.get(), image._data.get(), copySize );
    }

    Sprite::Sprite( Sprite && sprite ) noexcept
        : Image( std::move( sprite ) )
    {
        std::swap( _x, sprite._x );
        std::swap( _y, sprite._y );
    }

    Sprite & Sprite::operator=( const Sprite & sprite )
    {
        if ( this == &sprite ) {
            return *this;
        }

        Image::operator=( sprite );

        _x = sprite._x;
        _y = sprite._y;

        return *this;
    }

    Sprite & Sprite::operator=( Sprite && sprite ) noexcept
    {
        if ( this == &sprite ) {
            return *this;
        }

        Image::operator=( std::move( sprite ) );

        std::swap( _x, sprite._x );
        std::swap( _y, sprite._y );

        return *this;
    }

    Sprite & Sprite::operator=( Image && image ) noexcept
    {
        Image::operator=( std::move( image ) );

        _x = 0;
        _y = 0;

        return *this;
    }

    void Sprite::setPosition( const int32_t x_, const int32_t y_ )
    {
        _x = x_;
        _y = y_;
    }

    namespace
    {
        // Returns true if the image's backing buffer is sized larger than width()/height()
        // (the physical-resolution Display case). Driven by the bufferStride/bufferHeight
        // virtuals so any future "logical-vs-physical" Image subclass also takes this path.
        inline bool isPhysicalBuffer( const Image & image )
        {
            return image.bufferStride() != image.width() || image.bufferHeight() != image.height();
        }

        // Capture a physical-pixel rect of a physical-resolution RGBA source into _copy.
        // _copy is allocated at the matching physical dimensions so capture+restore round-trips
        // at full physical fidelity (no downscale on capture, no upscale on restore).
        void capturePhysicalCopy( const Image & src, const int32_t x, const int32_t y, const int32_t w, const int32_t h, Image & dst )
        {
            const float scale = src.physicalScale();
            const int32_t srcStride = src.bufferStride();
            const int32_t physX = static_cast<int32_t>( static_cast<float>( x ) * scale );
            const int32_t physY = static_cast<int32_t>( static_cast<float>( y ) * scale );
            const int32_t physW = static_cast<int32_t>( static_cast<float>( w ) * scale );
            const int32_t physH = static_cast<int32_t>( static_cast<float>( h ) * scale );
            if ( physW <= 0 || physH <= 0 ) {
                dst = Image();
                return;
            }
            dst = Image( physW, physH, ImageFormat::RGBA_32BIT );
            const uint8_t * srcBase = src.image();
            uint8_t * dstBase = dst.image();
            for ( int32_t row = 0; row < physH; ++row ) {
                memcpy( dstBase + static_cast<ptrdiff_t>( row ) * physW * 4,
                        srcBase + ( static_cast<ptrdiff_t>( physY + row ) * srcStride + physX ) * 4, static_cast<size_t>( physW ) * 4 );
            }
        }

        void restorePhysicalCopy( const Image & copy, const int32_t x, const int32_t y, const int32_t w, const int32_t h, Image & dst )
        {
            if ( copy.empty() ) {
                return;
            }
            const float scale = dst.physicalScale();
            const int32_t dstStride = dst.bufferStride();
            const int32_t physX = static_cast<int32_t>( static_cast<float>( x ) * scale );
            const int32_t physY = static_cast<int32_t>( static_cast<float>( y ) * scale );
            const int32_t physW = static_cast<int32_t>( static_cast<float>( w ) * scale );
            const int32_t physH = static_cast<int32_t>( static_cast<float>( h ) * scale );
            const uint8_t * srcBase = copy.image();
            uint8_t * dstBase = dst.image();
            // copy was allocated at this same physical extent; if dimensions don't match
            // (resolution change in flight), bail out.
            if ( copy.width() != physW || copy.height() != physH ) {
                return;
            }
            for ( int32_t row = 0; row < physH; ++row ) {
                memcpy( dstBase + ( static_cast<ptrdiff_t>( physY + row ) * dstStride + physX ) * 4,
                        srcBase + static_cast<ptrdiff_t>( row ) * physW * 4, static_cast<size_t>( physW ) * 4 );
            }
        }
    }

    ImageRestorer::ImageRestorer( Image & image )
        : _image( image )
        , _width( image.width() )
        , _height( image.height() )
    {
        _updateRoi();

        if ( _image.singleLayer() ) {
            _copy._disableTransformLayer();
        }
        if ( _image.format() == ImageFormat::RGBA_32BIT && isPhysicalBuffer( _image ) ) {
            // Physical-resolution Display: capture at physical-pixel resolution so restore
            // is a full-fidelity byte memcpy back into the physical buffer.
            capturePhysicalCopy( _image, _x, _y, _width, _height, _copy );
            _captureIndexed();
        }
        else if ( _image.format() == ImageFormat::RGBA_32BIT ) {
            _copy = Image( _width, _height, ImageFormat::RGBA_32BIT );
            Copy( _image, _x, _y, _copy, 0, 0, _width, _height );
        }
        else {
            _copy.resize( _width, _height );
            Copy( _image, _x, _y, _copy, 0, 0, _width, _height );
        }
    }

    ImageRestorer::ImageRestorer( Image & image, const int32_t x_, const int32_t y_, const int32_t width, const int32_t height )
        : _image( image )
        , _x( x_ )
        , _y( y_ )
        , _width( width )
        , _height( height )
    {
        _updateRoi();

        if ( _image.singleLayer() ) {
            _copy._disableTransformLayer();
        }
        if ( _image.format() == ImageFormat::RGBA_32BIT && isPhysicalBuffer( _image ) ) {
            capturePhysicalCopy( _image, _x, _y, _width, _height, _copy );
            _captureIndexed();
        }
        else if ( _image.format() == ImageFormat::RGBA_32BIT ) {
            _copy = Image( _width, _height, ImageFormat::RGBA_32BIT );
            Copy( _image, _x, _y, _copy, 0, 0, _width, _height );
        }
        else {
            _copy.resize( _width, _height );
            Copy( _image, _x, _y, _copy, 0, 0, _width, _height );
        }
    }

    void ImageRestorer::update( const int32_t x_, const int32_t y_, const int32_t width, const int32_t height )
    {
        _isRestored = false;
        _x = x_;
        _y = y_;
        _width = width;
        _height = height;
        _updateRoi();

        if ( _image.format() == ImageFormat::RGBA_32BIT && isPhysicalBuffer( _image ) ) {
            capturePhysicalCopy( _image, _x, _y, _width, _height, _copy );
            _captureIndexed();
        }
        else if ( _image.format() == ImageFormat::RGBA_32BIT ) {
            _copy = Image( _width, _height, ImageFormat::RGBA_32BIT );
            Copy( _image, _x, _y, _copy, 0, 0, _width, _height );
        }
        else {
            _copy.resize( _width, _height );
            Copy( _image, _x, _y, _copy, 0, 0, _width, _height );
        }
    }

    void ImageRestorer::restore()
    {
        _isRestored = true;
        if ( _image.format() == ImageFormat::RGBA_32BIT && isPhysicalBuffer( _image ) ) {
            restorePhysicalCopy( _copy, _x, _y, _width, _height, _image );
            _restoreIndexed();
            return;
        }
        Copy( _copy, 0, 0, _image, _x, _y, _width, _height );
    }

    void ImageRestorer::_captureIndexed()
    {
        const uint8_t * idx = _image.indexedBuffer();
        const uint8_t * mask = _image.maskBuffer();
        if ( idx == nullptr || mask == nullptr || _width <= 0 || _height <= 0 ) {
            _indexedCopy.clear();
            _maskCopy.clear();
            return;
        }
        const int32_t stride = _image.indexedStride();
        const size_t bytes = static_cast<size_t>( _width ) * static_cast<size_t>( _height );
        _indexedCopy.resize( bytes );
        _maskCopy.resize( bytes );
        for ( int32_t row = 0; row < _height; ++row ) {
            const ptrdiff_t srcOff = static_cast<ptrdiff_t>( _y + row ) * stride + _x;
            const ptrdiff_t dstOff = static_cast<ptrdiff_t>( row ) * _width;
            std::memcpy( _indexedCopy.data() + dstOff, idx + srcOff, static_cast<size_t>( _width ) );
            std::memcpy( _maskCopy.data() + dstOff, mask + srcOff, static_cast<size_t>( _width ) );
        }
    }

    void ImageRestorer::_restoreIndexed()
    {
        if ( _indexedCopy.empty() || _maskCopy.empty() ) {
            return;
        }
        uint8_t * idx = _image.indexedBuffer();
        uint8_t * mask = _image.maskBuffer();
        if ( idx == nullptr || mask == nullptr ) {
            return;
        }
        const int32_t stride = _image.indexedStride();
        for ( int32_t row = 0; row < _height; ++row ) {
            const ptrdiff_t dstOff = static_cast<ptrdiff_t>( _y + row ) * stride + _x;
            const ptrdiff_t srcOff = static_cast<ptrdiff_t>( row ) * _width;
            std::memcpy( idx + dstOff, _indexedCopy.data() + srcOff, static_cast<size_t>( _width ) );
            std::memcpy( mask + dstOff, _maskCopy.data() + srcOff, static_cast<size_t>( _width ) );
        }
        _image.markIndexedDirty( { _x, _y, _width, _height } );
    }

    void ImageRestorer::_updateRoi()
    {
        if ( _width < 0 ) {
            _width = 0;
        }

        if ( _height < 0 ) {
            _height = 0;
        }

        if ( _x < 0 ) {
            const int32_t offset = -_x;
            _x = 0;
            _width = _width < offset ? 0 : _width - offset;
        }

        if ( _y < 0 ) {
            const int32_t offset = -_y;
            _y = 0;
            _height = _height < offset ? 0 : _height - offset;
        }

        if ( _x >= _image.width() || _y >= _image.height() ) {
            _x = 0;
            _y = 0;
            _width = 0;
            _height = 0;
            return;
        }

        if ( _x + _width > _image.width() ) {
            const int32_t offsetX = _x + _width - _image.width();
            if ( offsetX >= _width ) {
                _x = 0;
                _y = 0;
                _width = 0;
                _height = 0;
                return;
            }
            _width -= offsetX;
        }

        if ( _y + _height > _image.height() ) {
            const int32_t offsetY = _y + _height - _image.height();
            if ( offsetY >= _height ) {
                _x = 0;
                _y = 0;
                _width = 0;
                _height = 0;
                return;
            }
            _height -= offsetY;
        }
    }

    // ===== Anonymous-namespace internal helpers for RGBA paths =====
    namespace
    {
        // Indexed src -> RGBA out, no scale, with optional flip and transform handling.
        //
        // Display fast path (Phase 3): when the destination owns a game-resolution indexed
        // buffer, write 1 byte per game pixel directly to that buffer instead of expanding
        // each pixel into a scale² physical RGBA block. The GPU composite shader handles the
        // upscale + palette LUT. Shadow pixels (transform 2..5) still need RGBA work — they
        // resolve the existing indexed pixel through the palette, darken, and write into the
        // RGBA buffer (and clear the indexed cell so the sentinel rule picks RGBA).
        void BlitIndexedToRGBAOutput( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                      const bool flip )
        {
            assert( in.format() == ImageFormat::INDEXED_8BIT );
            assert( out.format() == ImageFormat::RGBA_32BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            const int32_t widthIn = in.width();
            const bool hasTransform = !in.singleLayer();
            const int32_t inDir = flip ? -1 : 1;

            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();

            uint8_t * idxBase = out.indexedBuffer();
            uint8_t * maskBase = out.maskBuffer();
            const bool useIndexedFastPath = ( idxBase != nullptr && maskBase != nullptr );
            const int32_t idxStride = useIndexedFastPath ? out.indexedStride() : 0;

            const uint8_t * imageInY = in.image() + ( static_cast<ptrdiff_t>( inY ) * widthIn ) + ( flip ? ( widthIn - 1 - inX ) : inX );
            const uint8_t * transformInY = hasTransform
                                               ? ( in.transform() + ( static_cast<ptrdiff_t>( inY ) * widthIn ) + ( flip ? ( widthIn - 1 - inX ) : inX ) )
                                               : nullptr;

            for ( int32_t row = 0; row < height; ++row ) {
                const uint8_t * imgIn = imageInY;
                const uint8_t * trIn = transformInY;
                const ptrdiff_t rowOff = static_cast<ptrdiff_t>( outY + row ) * idxStride + outX;
                uint8_t * idxOut = useIndexedFastPath ? ( idxBase + rowOff ) : nullptr;
                uint8_t * maskOut = useIndexedFastPath ? ( maskBase + rowOff ) : nullptr;

                for ( int32_t col = 0; col < width; ++col, imgIn += inDir ) {
                    if ( hasTransform ) {
                        if ( *trIn == 1 ) {
                            // Transparent — leave indexed/mask alone (preserves the slot
                            // background painted earlier in the frame).
                            trIn += inDir;
                            if ( idxOut ) {
                                ++idxOut;
                                ++maskOut;
                            }
                            continue;
                        }
                        if ( *trIn > 1 && *trIn < 14 ) {
                            // Transform-table lookup — covers shadow (2-5) AND translucency
                            // (6-13). The pre-Phase-3 engine handled all of these the same
                            // way: dst = transformTable[trIn * 256 + dst]. Air Elemental,
                            // ghost-style sprites etc. use IDs 6-13 for varying-strength
                            // translucency; without this branch their pixels fell through
                            // to the source-image write below and the transparency was lost.
                            //
                            // Two cases by mask:
                            //   mask=255 (indexed pixel): remap idx through transformTable.
                            //     Works for both shadows (palette-quantised dim) and trans-
                            //     lucency (palette-quantised blend with the dst). Idempotent
                            //     in the normal "tile repainted before sprite each frame"
                            //     order, which is what redraw cycles do.
                            //   mask=0 (RGBA pixel): only shadows (2-5) have a closed-form
                            //     equivalent — write the transform id into idx as a shadow
                            //     flag, the shader multiplies by shadowFactor[idx] at sample
                            //     time. Translucency (6-13) has no closed-form RGBA factor
                            //     (it's a palette LUT), so we leave the RGBA pixel alone —
                            //     acceptable because translucent sprites land on the
                            //     indexed battlefield ground far more often than on RGBA
                            //     hi-res content.
                            if ( useIndexedFastPath ) {
                                if ( *maskOut != 0 ) {
                                    *idxOut = transformTable[static_cast<size_t>( *trIn ) * 256 + *idxOut];
                                }
                                else if ( *trIn <= 5 ) {
                                    *idxOut = *trIn;
                                }
                                else {
                                    // mask=0 + translucency (6-13): destination is genuine RGBA
                                    // (e.g. battlefield ground, video frame, hi-res monster). The
                                    // legacy indexed engine handled this by reverse-looking-up
                                    // the destination's palette index and remapping it through
                                    // transformTable; do the same, then promote the cell to
                                    // mask=255 so the GPU shader resolves palette[idx]. Without
                                    // this branch Air-Elemental / ghost-style sprites disappear
                                    // entirely against RGBA-painted backgrounds — happens on
                                    // every battle because Battle::Interface::_copyFullSurface()
                                    // splats _battleGroundRGBA over Display each frame.
                                    const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                                    const uint8_t * srcPx = outBase + ( static_cast<ptrdiff_t>( pb.pYStart ) * bufStride + pb.pXStart ) * 4;
                                    if ( srcPx[3] != 0 ) {
                                        const uint8_t srcIdx = GetPALColorId( srcPx[0] >> 2, srcPx[1] >> 2, srcPx[2] >> 2 );
                                        *idxOut = transformTable[static_cast<size_t>( *trIn ) * 256 + srcIdx];
                                        *maskOut = 255;
                                    }
                                }
                            }
                            else if ( *trIn <= 5 ) {
                                // Non-Display RGBA target — fall back to the existing in-place
                                // physical-pixel multiply for shadows. Translucency on a
                                // non-Display RGBA target is a no-op (rare and historically
                                // never supported on this code path either).
                                const float f = shadowFactor[*trIn];
                                const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                                shadeRGBABlock( outBase, pb, bufStride, f );
                            }
                            trIn += inDir;
                            if ( idxOut ) {
                                ++idxOut;
                                ++maskOut;
                            }
                            continue;
                        }
                        trIn += inDir;
                    }

                    if ( useIndexedFastPath ) {
                        *idxOut = *imgIn;
                        *maskOut = 255;
                        ++idxOut;
                        ++maskOut;
                    }
                    else {
                        uint8_t r;
                        uint8_t g;
                        uint8_t b;
                        paletteIdxToRGBA( *imgIn, r, g, b );
                        const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                        fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                    }
                }

                imageInY += widthIn;
                if ( transformInY ) {
                    transformInY += widthIn;
                }
            }

            if ( useIndexedFastPath ) {
                out.markIndexedDirty( { outX, outY, width, height } );
            }
        }

        // Indexed src -> RGBA out with src_over alpha (alpha 1..254).
        void AlphaBlitIndexedToRGBAOutput( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                           const uint8_t alpha, const bool flip )
        {
            assert( in.format() == ImageFormat::INDEXED_8BIT );
            assert( out.format() == ImageFormat::RGBA_32BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            // Phase 3: src_over alpha blend produces a non-palette RGB result that can't
            // be represented as an indexed value, so we must write through RGBA. The blend
            // reads the existing dst RGBA, so any game pixel currently resident only in
            // the indexed channel must be materialized to RGBA first — otherwise the
            // blend baseline is the stale RGBA leftovers (black). Used for the castle
            // building fade-in animation, hero pickup fades, etc.
            materializeIndexedRoi( out, outX, outY, width, height );

            const int32_t widthIn = in.width();
            const bool hasTransform = !in.singleLayer();
            const int32_t inDir = flip ? -1 : 1;
            const float alphaF = static_cast<float>( alpha ) / 255.0f;

            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();

            const uint8_t * imageInY = in.image() + ( static_cast<ptrdiff_t>( inY ) * widthIn ) + ( flip ? ( widthIn - 1 - inX ) : inX );
            const uint8_t * transformInY = hasTransform
                                               ? ( in.transform() + ( static_cast<ptrdiff_t>( inY ) * widthIn ) + ( flip ? ( widthIn - 1 - inX ) : inX ) )
                                               : nullptr;

            for ( int32_t row = 0; row < height; ++row ) {
                const uint8_t * imgIn = imageInY;
                const uint8_t * trIn = transformInY;

                for ( int32_t col = 0; col < width; ++col, imgIn += inDir ) {
                    if ( hasTransform ) {
                        if ( *trIn == 1 ) {
                            trIn += inDir;
                            continue;
                        }
                        if ( *trIn > 1 && *trIn <= 5 ) {
                            const float baseF = shadowFactor[*trIn];
                            const float f = 1.0f - ( 1.0f - baseF ) * alphaF;
                            const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                            shadeRGBABlock( outBase, pb, bufStride, f );
                            trIn += inDir;
                            continue;
                        }
                        trIn += inDir;
                    }

                    uint8_t srcR;
                    uint8_t srcG;
                    uint8_t srcB;
                    paletteIdxToRGBA( *imgIn, srcR, srcG, srcB );

                    const float invA = 1.0f - alphaF;
                    const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                    for ( int32_t py = pb.pYStart; py < pb.pYEnd; ++py ) {
                        uint8_t * dstRow = outBase + ( static_cast<ptrdiff_t>( py ) * bufStride + pb.pXStart ) * 4;
                        for ( int32_t px = pb.pXStart; px < pb.pXEnd; ++px, dstRow += 4 ) {
                            dstRow[0] = static_cast<uint8_t>( static_cast<float>( srcR ) * alphaF + static_cast<float>( dstRow[0] ) * invA );
                            dstRow[1] = static_cast<uint8_t>( static_cast<float>( srcG ) * alphaF + static_cast<float>( dstRow[1] ) * invA );
                            dstRow[2] = static_cast<uint8_t>( static_cast<float>( srcB ) * alphaF + static_cast<float>( dstRow[2] ) * invA );
                            dstRow[3] = std::max<uint8_t>( dstRow[3], alpha );
                        }
                    }
                }

                imageInY += widthIn;
                if ( transformInY ) {
                    transformInY += widthIn;
                }
            }
        }

        // RGBA src -> RGBA out blit (no scaling, optional flip), src_over alpha for fully opaque pixels = copy.
        // Iterates in game pixels; reads in at game-coord stride and writes the destination as a
        // physical-pixel block per game pixel when out's physicalScale > 1 (Display).
        void BlitRGBAToRGBAOutput( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                   const bool flip )
        {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            assert( out.format() == ImageFormat::RGBA_32BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            uint8_t * outMaskBase = out.maskBuffer();
            uint8_t * outIdxBase = out.indexedBuffer();
            const int32_t outIdxStride = ( outMaskBase != nullptr ) ? out.indexedStride() : 0;

            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * inBase = in.image();

            for ( int32_t y = 0; y < height; ++y ) {
                const int32_t srcGameY = inY + y;
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( srcGameY ) * inScale );
                const uint8_t * srcRow = inBase + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;

                for ( int32_t x = 0; x < width; ++x ) {
                    const int32_t srcGameX = flip ? ( inX + width - 1 - x ) : ( inX + x );
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcGameX ) * inScale );
                    const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                    if ( srcPx[3] == 0 ) {
                        continue;
                    }
                    const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                    fillRGBABlock( outBase, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcPx[3] );

                    // Mask-clear only at game pixels we actually paint. Transparent source
                    // pixels leave the slot frame's indexed channel intact.
                    if ( outMaskBase != nullptr ) {
                        const ptrdiff_t off = static_cast<ptrdiff_t>( outY + y ) * outIdxStride + ( outX + x );
                        outMaskBase[off] = 0;
                        outIdxBase[off] = 0;
                    }
                }
            }
            if ( outMaskBase != nullptr ) {
                out.markIndexedDirty( { outX, outY, width, height } );
            }
        }

        // RGBA src -> RGBA out, alpha-blended. Indexed bbox is cleared so the destination's
        // sentinel rule resolves through the freshly blended RGBA result.
        void AlphaBlitRGBAToRGBAOutput( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                        const uint8_t alpha, const bool flip )
        {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            assert( out.format() == ImageFormat::RGBA_32BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            // Phase 3: src_over alpha blend reads the destination — materialize indexed
            // pixels into RGBA first so the blend baseline is the correct visible colour.
            // Clears the mask at touched pixels.
            materializeIndexedRoi( out, outX, outY, width, height );

            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * inBase = in.image();

            for ( int32_t y = 0; y < height; ++y ) {
                const int32_t srcGameY = inY + y;
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( srcGameY ) * inScale );
                const uint8_t * srcRow = inBase + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;

                for ( int32_t x = 0; x < width; ++x ) {
                    const int32_t srcGameX = flip ? ( inX + width - 1 - x ) : ( inX + x );
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcGameX ) * inScale );
                    const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                    if ( srcPx[3] == 0 ) {
                        continue;
                    }
                    const uint32_t srcA = ( static_cast<uint32_t>( srcPx[3] ) * alpha ) / 255;
                    if ( srcA == 0 ) {
                        continue;
                    }
                    const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                    blendRGBABlock( outBase, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcA );
                }
            }
        }

        // Indexed src -> RGBA out, plain memcpy of palette[]->RGBA per pixel (no transform, no flip).
        // Phase 3 Display fast path: write 1 byte per game pixel to the indexed buffer (memcpy
        // by row) and skip the scale² block expansion entirely.
        void CopyIndexedToRGBAOutput( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height )
        {
            assert( in.format() == ImageFormat::INDEXED_8BIT );
            assert( out.format() == ImageFormat::RGBA_32BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            const int32_t widthIn = in.width();

            uint8_t * idxBase = out.indexedBuffer();
            uint8_t * maskBase = out.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = out.indexedStride();
                const uint8_t * srcRow = in.image() + ( static_cast<ptrdiff_t>( inY ) * widthIn ) + inX;
                for ( int32_t row = 0; row < height; ++row ) {
                    const ptrdiff_t off = static_cast<ptrdiff_t>( outY + row ) * idxStride + outX;
                    std::memcpy( idxBase + off, srcRow, static_cast<size_t>( width ) );
                    std::memset( maskBase + off, 255, static_cast<size_t>( width ) );
                    srcRow += widthIn;
                }
                out.markIndexedDirty( { outX, outY, width, height } );
                return;
            }

            // Non-Display RGBA target — keep the existing physical-resolution write path.
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * srcRow = in.image() + ( static_cast<ptrdiff_t>( inY ) * widthIn ) + inX;
            for ( int32_t row = 0; row < height; ++row ) {
                const uint8_t * src = srcRow;
                for ( int32_t col = 0; col < width; ++col, ++src ) {
                    uint8_t r;
                    uint8_t g;
                    uint8_t b;
                    paletteIdxToRGBA( *src, r, g, b );
                    const PhysicalBlock pb = toPhysicalBlock( outX + col, outY + row, scale, bufStride, bufHeight );
                    fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                }
                srcRow += widthIn;
            }
        }

        // RGBA src -> RGBA out copy. Treats coords as game coords on both sides; reads source
        // at its physical-pixel stride and writes destination as a physical-pixel block per
        // game pixel when out is a physical-resolution buffer (Display). For matching scales
        // (the common scratch -> scratch case) collapses to the previous memcpy fast path.
        void CopyRGBAToRGBAOutput( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height )
        {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            assert( out.format() == ImageFormat::RGBA_32BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            // Phase 3: this primitive overwrites every dst pixel with src content (the src
            // is read at physical-pixel scale, not transparency-aware). The whole destination
            // bbox becomes RGBA-resolved, so a coarse mask clear is correct here — there are
            // no "transparent pixels" that should preserve the underlying indexed channel.
            clearIndexedBboxOnDisplay( out, outX, outY, width, height );

            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * inBase = in.image();

            // Fast path: identical scale on both sides — copy at physical-pixel resolution.
            // Both scales are derived from physical/game integer ratios so bit-exact
            // equality is correct here; suppress the FP-equality warning.
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
            if ( inScale == scale ) {
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif
                const int32_t physInX = static_cast<int32_t>( static_cast<float>( inX ) * inScale );
                const int32_t physInY = static_cast<int32_t>( static_cast<float>( inY ) * inScale );
                const int32_t physOutX = static_cast<int32_t>( static_cast<float>( outX ) * scale );
                const int32_t physOutY = static_cast<int32_t>( static_cast<float>( outY ) * scale );
                const int32_t physW = static_cast<int32_t>( static_cast<float>( width ) * scale );
                const int32_t physH = static_cast<int32_t>( static_cast<float>( height ) * scale );
                for ( int32_t y = 0; y < physH; ++y ) {
                    const uint8_t * srcRow = inBase + ( static_cast<ptrdiff_t>( physInY + y ) * inStride + physInX ) * 4;
                    uint8_t * dstRow = outBase + ( static_cast<ptrdiff_t>( physOutY + y ) * bufStride + physOutX ) * 4;
                    memcpy( dstRow, srcRow, static_cast<size_t>( physW ) * 4 );
                }
                return;
            }

            // Slow path: scales differ — sample source at game-coord nearest, write block on dest.
            for ( int32_t y = 0; y < height; ++y ) {
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( inY + y ) * inScale );
                const uint8_t * srcRow = inBase + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;

                for ( int32_t x = 0; x < width; ++x ) {
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( inX + x ) * inScale );
                    const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                    const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                    fillRGBABlock( outBase, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcPx[3] );
                }
            }
        }

        // RGBA src -> indexed out (palette quantization). The source can be a Display with a
        // physical-resolution backing buffer; we sample one physical pixel per game pixel via
        // in.physicalScale() / in.bufferStride() (nearest-neighbour). out is always indexed at
        // game res, so output coords stay 1:1.
        void BlitRGBAToIndexed( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const bool flip )
        {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            assert( out.format() == ImageFormat::INDEXED_8BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const int32_t widthOut = out.width();

            uint8_t * imageOutY = out.image() + ( static_cast<ptrdiff_t>( outY ) * widthOut ) + outX;
            uint8_t * transformOutY = out.singleLayer() ? nullptr : ( out.transform() + ( static_cast<ptrdiff_t>( outY ) * widthOut ) + outX );

            for ( int32_t y = 0; y < height; ++y ) {
                uint8_t * imageOutX = imageOutY;
                uint8_t * transformOutX = transformOutY;
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( inY + y ) * inScale );

                for ( int32_t x = 0; x < width; ++x ) {
                    const int32_t srcGameX = flip ? ( inX + width - 1 - x ) : ( inX + x );
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcGameX ) * inScale );
                    const ptrdiff_t pixelOffset = ( static_cast<ptrdiff_t>( srcPhysY ) * inStride + srcPhysX ) * 4;
                    const uint8_t * px = in.image() + pixelOffset;

                    if ( px[3] == 0 ) {
                        ++imageOutX;
                        if ( transformOutX ) {
                            ++transformOutX;
                        }
                        continue;
                    }

                    // GetPALColorId expects 6-bit RGB (0..63) — its internal lookup table
                    // is sized 64×64×64. Display RGBA bytes are 8-bit (0..255); shifting
                    // right by 2 converts to the 6-bit scale.
                    *imageOutX = GetPALColorId( px[0] >> 2, px[1] >> 2, px[2] >> 2 );
                    if ( transformOutX ) {
                        *transformOutX = 0;
                        ++transformOutX;
                    }
                    ++imageOutX;
                }

                imageOutY += widthOut;
                if ( transformOutY ) {
                    transformOutY += widthOut;
                }
            }
        }

        // RGBA src -> indexed out with src_over alpha. Source-side reads are physical-pixel
        // (nearest sample) when the source is a physical-resolution Display.
        void AlphaBlitRGBAToIndexed( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                     const uint8_t alphaValue, const bool flip )
        {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            assert( out.format() == ImageFormat::INDEXED_8BIT );

            if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
                return;
            }

            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const int32_t widthOut = out.width();
            const uint8_t * gamePalette = getGamePalette();

            uint8_t * imageOutY = out.image() + ( static_cast<ptrdiff_t>( outY ) * widthOut ) + outX;

            for ( int32_t y = 0; y < height; ++y ) {
                uint8_t * imageOutX = imageOutY;
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( inY + y ) * inScale );

                for ( int32_t x = 0; x < width; ++x, ++imageOutX ) {
                    const int32_t srcGameX = flip ? ( inX + width - 1 - x ) : ( inX + x );
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcGameX ) * inScale );
                    const ptrdiff_t pixelOffset = ( static_cast<ptrdiff_t>( srcPhysY ) * inStride + srcPhysX ) * 4;
                    const uint8_t * px = in.image() + pixelOffset;

                    const uint8_t srcAlpha = px[3];
                    if ( srcAlpha == 0 ) {
                        continue;
                    }
                    const uint32_t combinedAlpha = ( static_cast<uint32_t>( srcAlpha ) * alphaValue ) / 255;
                    if ( combinedAlpha == 0 ) {
                        continue;
                    }
                    const uint32_t behindAlpha = 255 - combinedAlpha;
                    const uint8_t * outPAL = gamePalette + ( static_cast<ptrdiff_t>( *imageOutX ) * 3 );

                    // outPAL values are 6-bit (0..63, gamePalette format); px values are 8-bit
                    // (0..255, RGBA Display). Shift px to 6-bit so the alpha blend stays in a
                    // single colour scale and the result fits GetPALColorId's 6-bit input.
                    const uint32_t red = ( static_cast<uint32_t>( px[0] >> 2 ) * combinedAlpha ) + ( static_cast<uint32_t>( outPAL[0] ) * behindAlpha );
                    const uint32_t green = ( static_cast<uint32_t>( px[1] >> 2 ) * combinedAlpha ) + ( static_cast<uint32_t>( outPAL[1] ) * behindAlpha );
                    const uint32_t blue = ( static_cast<uint32_t>( px[2] >> 2 ) * combinedAlpha ) + ( static_cast<uint32_t>( outPAL[2] ) * behindAlpha );

                    *imageOutX = GetPALColorId( static_cast<uint8_t>( red / 255 ), static_cast<uint8_t>( green / 255 ), static_cast<uint8_t>( blue / 255 ) );
                }

                imageOutY += widthOut;
            }
        }
    } // anonymous

    // ===== Public drawing primitives =====

    void addGradientShadow( const Sprite & in, Image & out, const Point & outPos, const Point & shadowOffset )
    {
        if ( in.empty() || out.empty() || ( shadowOffset.x == 0 && shadowOffset.y == 0 ) || ( outPos.x < 0 ) || ( outPos.y < 0 ) ) {
            return;
        }

        // RGBA-output path: darken affected pixels by shadowFactor (block-expanded for Display).
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            const int32_t outWidth = out.width();
            const int32_t outHeight = out.height();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            const int32_t inWidth = in.width();
            const int32_t inHeight = in.height();
            const int32_t shadowOffsetX = std::min<int32_t>( shadowOffset.x, 0 );
            const int32_t shadowOffsetY = std::min<int32_t>( shadowOffset.y, 0 );
            const int32_t baseX = outPos.x + shadowOffsetX + in.x();
            const int32_t baseY = outPos.y + shadowOffsetY + in.y();

            const int32_t absOffsetX = std::abs( shadowOffset.x );
            const int32_t absOffsetY = std::abs( shadowOffset.y );

            std::vector<Point> shadowLine;
            shadowLine.reserve( std::max( absOffsetX, absOffsetY ) + 1 );
            if ( shadowOffset.x == 0 ) {
                const int32_t maxY = absOffsetY + shadowOffsetY;
                for ( int32_t y = shadowOffsetY; y <= maxY; ++y ) {
                    shadowLine.emplace_back( 0, y );
                }
            }
            else {
                const double slopeFactor = static_cast<double>( shadowOffset.y ) / shadowOffset.x;
                if ( absOffsetX >= absOffsetY ) {
                    const int32_t maxX = absOffsetX + shadowOffsetX;
                    for ( int32_t x = shadowOffsetX; x <= maxX; ++x ) {
                        shadowLine.emplace_back( x, static_cast<int32_t>( std::round( x * slopeFactor ) ) );
                    }
                }
                else {
                    const int32_t maxY = absOffsetY + shadowOffsetY;
                    for ( int32_t y = shadowOffsetY; y <= maxY; ++y ) {
                        shadowLine.emplace_back( static_cast<int32_t>( std::round( y / slopeFactor ) ), y );
                    }
                }
            }

            const int32_t maxX = inWidth + absOffsetX;
            const int32_t maxY = inHeight + absOffsetY;
            const uint8_t * transformIn = in.singleLayer() ? nullptr : in.transform();

            const auto isTransparent = [inWidth, inHeight, transformIn]( const int32_t offsetX, const int32_t offsetY ) {
                if ( ( offsetX < 0 ) || ( offsetY < 0 ) || ( offsetX >= inWidth ) || ( offsetY >= inHeight ) ) {
                    return true;
                }
                if ( transformIn == nullptr ) {
                    return false;
                }
                return ( *( transformIn + offsetX + static_cast<ptrdiff_t>( offsetY ) * inWidth ) == 1 );
            };

            // Phase 3: shadow encoded at the channel level for GPU-side dimming.
            //   mask=255 → remap idx through transformTable (palette-quantised dim).
            //   mask=0   → write transformTableId into idx as a shadow flag; shader
            //              multiplies RGBA by shadowFactor[id] at sample time.
            // Idempotent — same byte written each frame, no CPU multiply, no compound.
            uint8_t * idxBase = out.indexedBuffer();
            uint8_t * maskBase = out.maskBuffer();
            const int32_t idxStride = ( idxBase != nullptr ) ? out.indexedStride() : 0;
            bool anyMaskChange = false;

            for ( int32_t y = 0; y < maxY; ++y ) {
                const int32_t offsetY = y + shadowOffsetY;
                const int32_t dstY = baseY + y;
                if ( dstY < 0 || dstY >= outHeight ) {
                    continue;
                }
                for ( int32_t x = 0; x < maxX; ++x ) {
                    const int32_t offsetX = x + shadowOffsetX;
                    if ( !isTransparent( offsetX, offsetY ) ) {
                        continue;
                    }
                    uint8_t transformTableId = 6;
                    for ( const Point & shadowLineOffset : shadowLine ) {
                        if ( !isTransparent( offsetX - shadowLineOffset.x, offsetY - shadowLineOffset.y ) ) {
                            --transformTableId;
                            if ( transformTableId == 2 ) {
                                break;
                            }
                        }
                    }
                    if ( transformTableId == 6 ) {
                        continue;
                    }
                    const int32_t dstX = baseX + x;
                    if ( dstX < 0 || dstX >= outWidth ) {
                        continue;
                    }
                    if ( idxBase != nullptr ) {
                        const ptrdiff_t idxOff = static_cast<ptrdiff_t>( dstY ) * idxStride + dstX;
                        if ( maskBase[idxOff] != 0 ) {
                            idxBase[idxOff] = transformTable[static_cast<size_t>( transformTableId ) * 256 + idxBase[idxOff]];
                        }
                        else {
                            idxBase[idxOff] = transformTableId;
                        }
                        anyMaskChange = true;
                    }
                    else {
                        const float f = shadowFactor[transformTableId];
                        const PhysicalBlock pb = toPhysicalBlock( dstX, dstY, scale, bufStride, bufHeight );
                        shadeRGBABlock( out.image(), pb, bufStride, f );
                    }
                }
            }
            if ( anyMaskChange ) {
                out.markIndexedDirty( { baseX, baseY, maxX, maxY } );
            }
            return;
        }

        const int32_t outWidth = out.width();
        const int32_t inWidth = in.width();
        const int32_t inHeight = in.height();
        const int32_t shadowOffsetX = std::min<int32_t>( shadowOffset.x, 0 );
        const int32_t shadowOffsetY = std::min<int32_t>( shadowOffset.y, 0 );
        const int32_t outStartOffset = outPos.x + shadowOffsetX + in.x() + ( outPos.y + shadowOffsetY + in.y() ) * outWidth;

        assert( outStartOffset >= 0 && outWidth >= ( inWidth + outPos.x + std::max<int32_t>( shadowOffset.x, 0 ) )
                && out.height() >= ( inHeight + outPos.y + std::max<int32_t>( shadowOffset.y, 0 ) ) );

        const int32_t absOffsetX = std::abs( shadowOffset.x );
        const int32_t absOffsetY = std::abs( shadowOffset.y );

        std::vector<Point> shadowLine;
        shadowLine.reserve( std::max( absOffsetX, absOffsetY ) + 1 );

        if ( shadowOffset.x == 0 ) {
            const int32_t maxY = absOffsetY + shadowOffsetY;
            for ( int32_t y = shadowOffsetY; y <= maxY; ++y ) {
                shadowLine.emplace_back( 0, y );
            }
        }
        else {
            const double slopeFactor = static_cast<double>( shadowOffset.y ) / shadowOffset.x;
            if ( absOffsetX >= absOffsetY ) {
                const int32_t maxX = absOffsetX + shadowOffsetX;
                for ( int32_t x = shadowOffsetX; x <= maxX; ++x ) {
                    shadowLine.emplace_back( x, static_cast<int32_t>( std::round( x * slopeFactor ) ) );
                }
            }
            else {
                const int32_t maxY = absOffsetY + shadowOffsetY;
                for ( int32_t y = shadowOffsetY; y <= maxY; ++y ) {
                    shadowLine.emplace_back( static_cast<int32_t>( std::round( y / slopeFactor ) ), y );
                }
            }
        }

        const int32_t maxX = inWidth + absOffsetX;
        const int32_t maxY = inHeight + absOffsetY;

        const uint8_t * transformIn = in.singleLayer() ? nullptr : in.transform();
        uint8_t * transformOut = out.singleLayer() ? nullptr : ( out.transform() + outStartOffset );
        uint8_t * imageOut = out.image() + outStartOffset;

        const auto isTransparent = [inWidth, inHeight, transformIn]( const int32_t offsetX, const int32_t offsetY ) {
            if ( ( offsetX < 0 ) || ( offsetY < 0 ) || ( offsetX >= inWidth ) || ( offsetY >= inHeight ) ) {
                return true;
            }
            if ( transformIn == nullptr ) {
                return false;
            }
            return ( *( transformIn + offsetX + static_cast<ptrdiff_t>( offsetY ) * inWidth ) == 1 );
        };

        for ( int32_t y = 0; y < maxY; ++y ) {
            const int32_t offsetY = y + shadowOffsetY;
            for ( int32_t x = 0; x < maxX; ++x ) {
                const int32_t offsetX = x + shadowOffsetX;

                if ( !isTransparent( offsetX, offsetY ) ) {
                    continue;
                }

                uint8_t transformTableId = 6;
                for ( const Point & shadowLineOffset : shadowLine ) {
                    if ( !isTransparent( offsetX - shadowLineOffset.x, offsetY - shadowLineOffset.y ) ) {
                        --transformTableId;
                        if ( transformTableId == 2 ) {
                            break;
                        }
                    }
                }

                if ( transformTableId == 6 ) {
                    continue;
                }

                const int32_t outOffset = x + y * outWidth;

                if ( transformOut != nullptr ) {
                    uint8_t * transformOutX = transformOut + outOffset;

                    if ( *transformOutX == 0 ) {
                        uint8_t * imageOutX = imageOut + outOffset;
                        *imageOutX = *( transformTable + transformTableId * ptrdiff_t{ 256 } + *imageOutX );
                    }
                    else if ( *transformOutX > 1 && *transformOutX < 6 ) {
                        *transformOutX = ( *transformOutX < 2 + transformTableId ) ? 2 : ( *transformOutX - transformTableId );
                    }
                    else {
                        *transformOutX = transformTableId;
                    }
                }
                else {
                    uint8_t * imageOutX = imageOut + outOffset;
                    *imageOutX = *( transformTable + transformTableId * ptrdiff_t{ 256 } + *imageOutX );
                }
            }
        }
    }

    void addGradientShadowForArea( Image & out, const Point & outPos, const int32_t areaWidth, const int32_t areaHeight, const int32_t shadowOffset )
    {
        if ( out.empty() || outPos.x < 0 || outPos.y < 0 || shadowOffset < 1 ) {
            return;
        }

        // Render shadow at the left side of the area
        int32_t offsetY = outPos.y + shadowOffset;
        ApplyTransform( out, outPos.x - shadowOffset, offsetY, shadowOffset, 1, 5 );
        ++offsetY;
        ApplyTransform( out, outPos.x - shadowOffset, offsetY, 1, areaHeight, 5 );
        ApplyTransform( out, outPos.x - shadowOffset + 1, offsetY, shadowOffset - 1, 1, 4 );
        ++offsetY;
        ApplyTransform( out, outPos.x - shadowOffset + 1, offsetY, 1, areaHeight - 4, 4 );
        ApplyTransform( out, outPos.x - shadowOffset + 2, offsetY, shadowOffset - 2, 1, 3 );
        ++offsetY;
        ApplyTransform( out, outPos.x - shadowOffset + 2, offsetY, 1, areaHeight - 6, 3 );
        ApplyTransform( out, outPos.x - shadowOffset + 3, offsetY, shadowOffset - 3, areaHeight - shadowOffset - 3, 2 );

        // Render shadow at the bottom side of the area
        offsetY = outPos.y + areaHeight;
        const int32_t shadowBottomEdge = outPos.y + areaHeight + shadowOffset;
        ApplyTransform( out, outPos.x - shadowOffset + 3, offsetY, areaWidth - 6, shadowOffset - 3, 2 );
        ApplyTransform( out, outPos.x - shadowOffset + 2, shadowBottomEdge - 3, areaWidth - 4, 1, 3 );
        ApplyTransform( out, outPos.x - shadowOffset + areaWidth - 3, offsetY, 1, shadowOffset - 3, 3 );
        ApplyTransform( out, outPos.x - shadowOffset + 1, shadowBottomEdge - 2, areaWidth - 2, 1, 4 );
        ApplyTransform( out, outPos.x - shadowOffset + areaWidth - 2, offsetY, 1, shadowOffset - 2, 4 );
        ApplyTransform( out, outPos.x - shadowOffset, shadowBottomEdge - 1, areaWidth, 1, 5 );
        ApplyTransform( out, outPos.x - shadowOffset + areaWidth - 1, offsetY, 1, shadowOffset, 5 );
    }

    Sprite addShadow( const Sprite & in, const Point & shadowOffset, const uint8_t transformId )
    {
        if ( in.empty() || shadowOffset.x > 0 || shadowOffset.y < 0 ) {
            return in;
        }

        Sprite out = makeShadow( in, shadowOffset, transformId );
        Blit( in, out, -shadowOffset.x, 0 );

        return out;
    }

    void AlphaBlit( const Image & in, Image & out, const uint8_t alphaValue, const bool flip /* = false */ )
    {
        AlphaBlit( in, 0, 0, out, 0, 0, in.width(), in.height(), alphaValue, flip );
    }

    void AlphaBlit( const Image & in, Image & out, int32_t outX, int32_t outY, const uint8_t alphaValue, const bool flip /* = false */ )
    {
        AlphaBlit( in, 0, 0, out, outX, outY, in.width(), in.height(), alphaValue, flip );
    }

    void AlphaBlit( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const uint8_t alphaValue,
                    const bool flip /* = false */ )
    {
        if ( alphaValue == 0 ) {
            return;
        }

        if ( in.format() == ImageFormat::RGBA_32BIT ) {
            if ( out.format() == ImageFormat::RGBA_32BIT ) {
                if ( alphaValue == 255 ) {
                    BlitRGBAToRGBAOutput( in, inX, inY, out, outX, outY, width, height, flip );
                }
                else {
                    AlphaBlitRGBAToRGBAOutput( in, inX, inY, out, outX, outY, width, height, alphaValue, flip );
                }
                return;
            }
            if ( alphaValue == 255 ) {
                BlitRGBAToIndexed( in, inX, inY, out, outX, outY, width, height, flip );
            }
            else {
                AlphaBlitRGBAToIndexed( in, inX, inY, out, outX, outY, width, height, alphaValue, flip );
            }
            return;
        }

        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            if ( alphaValue == 255 ) {
                BlitIndexedToRGBAOutput( in, inX, inY, out, outX, outY, width, height, flip );
            }
            else {
                AlphaBlitIndexedToRGBAOutput( in, inX, inY, out, outX, outY, width, height, alphaValue, flip );
            }
            return;
        }

        if ( alphaValue == 255 ) {
            Blit( in, inX, inY, out, outX, outY, width, height, flip );
            return;
        }

        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();
        const uint8_t behindValue = 255 - alphaValue;
        const uint8_t * gamePalette = getGamePalette();

        if ( flip ) {
            const int32_t offsetInY = inY * widthIn + widthIn - 1 - inX;
            const uint8_t * imageInY = in.image() + offsetInY;

            const int32_t offsetOutY = outY * widthOut + outX;
            uint8_t * imageOutY = out.image() + offsetOutY;
            const uint8_t * imageOutYEnd = imageOutY + static_cast<ptrdiff_t>( height ) * widthOut;

            if ( in.singleLayer() ) {
                for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, imageOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;

                    for ( ; imageOutX != imageOutXEnd; --imageInX, ++imageOutX ) {
                        const uint8_t * inPAL = gamePalette + static_cast<ptrdiff_t>( *imageInX ) * 3;
                        const uint8_t * outPAL = gamePalette + static_cast<ptrdiff_t>( *imageOutX ) * 3;

                        const uint32_t red = static_cast<uint32_t>( *inPAL ) * alphaValue + static_cast<uint32_t>( *outPAL ) * behindValue;
                        const uint32_t green = static_cast<uint32_t>( *( inPAL + 1 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 1 ) ) * behindValue;
                        const uint32_t blue = static_cast<uint32_t>( *( inPAL + 2 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 2 ) ) * behindValue;
                        *imageOutX = GetPALColorId( static_cast<uint8_t>( red / 255 ), static_cast<uint8_t>( green / 255 ), static_cast<uint8_t>( blue / 255 ) );
                    }
                }
            }
            else {
                const uint8_t * transformInY = in.transform() + offsetInY;

                for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;

                    for ( ; imageOutX != imageOutXEnd; --imageInX, --transformInX, ++imageOutX ) {
                        if ( *transformInX == 1 ) {
                            continue;
                        }

                        uint8_t inValue = *imageInX;
                        if ( *transformInX > 1 ) {
                            inValue = *( transformTable + static_cast<ptrdiff_t>( *transformInX ) * 256 + *imageOutX );
                        }

                        const uint8_t * inPAL = gamePalette + static_cast<ptrdiff_t>( inValue ) * 3;
                        const uint8_t * outPAL = gamePalette + static_cast<ptrdiff_t>( *imageOutX ) * 3;

                        const uint32_t red = static_cast<uint32_t>( *inPAL ) * alphaValue + static_cast<uint32_t>( *outPAL ) * behindValue;
                        const uint32_t green = static_cast<uint32_t>( *( inPAL + 1 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 1 ) ) * behindValue;
                        const uint32_t blue = static_cast<uint32_t>( *( inPAL + 2 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 2 ) ) * behindValue;
                        *imageOutX = GetPALColorId( static_cast<uint8_t>( red / 255 ), static_cast<uint8_t>( green / 255 ), static_cast<uint8_t>( blue / 255 ) );
                    }
                }
            }
        }
        else {
            const int32_t offsetInY = inY * widthIn + inX;
            const uint8_t * imageInY = in.image() + offsetInY;

            uint8_t * imageOutY = out.image() + static_cast<ptrdiff_t>( outY ) * widthOut + outX;
            const uint8_t * imageInYEnd = imageInY + static_cast<ptrdiff_t>( height ) * widthIn;

            if ( in.singleLayer() ) {
                for ( ; imageInY != imageInYEnd; imageInY += widthIn, imageOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageInXEnd = imageInX + width;

                    for ( ; imageInX != imageInXEnd; ++imageInX, ++imageOutX ) {
                        const uint8_t * inPAL = gamePalette + static_cast<ptrdiff_t>( *imageInX ) * 3;
                        const uint8_t * outPAL = gamePalette + static_cast<ptrdiff_t>( *imageOutX ) * 3;

                        const uint32_t red = static_cast<uint32_t>( *inPAL ) * alphaValue + static_cast<uint32_t>( *outPAL ) * behindValue;
                        const uint32_t green = static_cast<uint32_t>( *( inPAL + 1 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 1 ) ) * behindValue;
                        const uint32_t blue = static_cast<uint32_t>( *( inPAL + 2 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 2 ) ) * behindValue;
                        *imageOutX = GetPALColorId( static_cast<uint8_t>( red / 255 ), static_cast<uint8_t>( green / 255 ), static_cast<uint8_t>( blue / 255 ) );
                    }
                }
            }
            else {
                const uint8_t * transformInY = in.transform() + offsetInY;

                for ( ; imageInY != imageInYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageInXEnd = imageInX + width;

                    for ( ; imageInX != imageInXEnd; ++imageInX, ++transformInX, ++imageOutX ) {
                        if ( *transformInX == 1 ) {
                            continue;
                        }

                        uint8_t inValue = *imageInX;
                        if ( *transformInX > 1 ) {
                            inValue = *( transformTable + static_cast<ptrdiff_t>( *transformInX ) * 256 + *imageOutX );
                        }

                        const uint8_t * inPAL = gamePalette + static_cast<ptrdiff_t>( inValue ) * 3;
                        const uint8_t * outPAL = gamePalette + static_cast<ptrdiff_t>( *imageOutX ) * 3;

                        const uint32_t red = static_cast<uint32_t>( *inPAL ) * alphaValue + static_cast<uint32_t>( *outPAL ) * behindValue;
                        const uint32_t green = static_cast<uint32_t>( *( inPAL + 1 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 1 ) ) * behindValue;
                        const uint32_t blue = static_cast<uint32_t>( *( inPAL + 2 ) ) * alphaValue + static_cast<uint32_t>( *( outPAL + 2 ) ) * behindValue;
                        *imageOutX = GetPALColorId( static_cast<uint8_t>( red / 255 ), static_cast<uint8_t>( green / 255 ), static_cast<uint8_t>( blue / 255 ) );
                    }
                }
            }
        }
    }

    void ApplyPalette( Image & image, const std::vector<uint8_t> & palette )
    {
        ApplyPalette( image, image, palette );
    }

    void ApplyPalette( const Image & in, Image & out, const std::vector<uint8_t> & palette )
    {
        if ( palette.size() != 256 ) {
            return;
        }
        ApplyRawPalette( in, 0, 0, out, 0, 0, in.width(), in.height(), palette.data() );
    }

    void ApplyPalette( Image & image, const uint8_t paletteId )
    {
        ApplyPalette( image, image, paletteId );
    }

    void ApplyPalette( const Image & in, Image & out, const uint8_t paletteId )
    {
        if ( paletteId > 15 ) {
            return;
        }
        ApplyRawPalette( in, 0, 0, out, 0, 0, in.width(), in.height(), transformTable + paletteId * 256 );
    }

    void ApplyPalette( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, uint8_t paletteId )
    {
        if ( paletteId > 15 ) {
            return;
        }
        ApplyRawPalette( in, inX, inY, out, outX, outY, width, height, transformTable + paletteId * 256 );
    }

    void ApplyPalette( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                       const std::vector<uint8_t> & palette )
    {
        if ( palette.size() != 256 ) {
            return;
        }
        ApplyRawPalette( in, inX, inY, out, outX, outY, width, height, palette.data() );
    }

    void ApplyAlpha( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const uint8_t alpha )
    {
        // RGBA in + RGBA out: directly scale RGB by alpha/255 (preserving the alpha channel).
        // This is the fast fade-out / fade-in path on the pure-RGBA Display.
        if ( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT ) {
            int32_t lInX = inX;
            int32_t lInY = inY;
            int32_t lOutX = outX;
            int32_t lOutY = outY;
            int32_t lW = width;
            int32_t lH = height;
            if ( !Verify( in, lInX, lInY, out, lOutX, lOutY, lW, lH ) ) {
                return;
            }

            // Phase 3: this is an in-place fade — multiplies the existing visible colour
            // by alpha. If the dst pixel currently lives only in the indexed channel,
            // materialize it to RGBA first so the multiply has the correct baseline.
            if ( &in == &out ) {
                materializeIndexedRoi( out, lOutX, lOutY, lW, lH );
            }
            else {
                clearIndexedBboxOnDisplay( out, lOutX, lOutY, lW, lH );
            }

            const float alphaF = static_cast<float>( alpha ) / 255.0f;
            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * inBase = in.image();

            for ( int32_t y = 0; y < lH; ++y ) {
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( lInY + y ) * inScale );
                const uint8_t * srcRow = inBase + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;
                for ( int32_t x = 0; x < lW; ++x ) {
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( lInX + x ) * inScale );
                    const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                    if ( srcPx[3] == 0 ) {
                        continue;
                    }
                    const uint8_t r = static_cast<uint8_t>( static_cast<float>( srcPx[0] ) * alphaF );
                    const uint8_t g = static_cast<uint8_t>( static_cast<float>( srcPx[1] ) * alphaF );
                    const uint8_t b = static_cast<uint8_t>( static_cast<float>( srcPx[2] ) * alphaF );
                    const PhysicalBlock pb = toPhysicalBlock( lOutX + x, lOutY + y, scale, bufStride, bufHeight );
                    fillRGBABlock( outBase, pb, bufStride, r, g, b, srcPx[3] );
                }
            }
            return;
        }

        std::vector<uint8_t> palette( 256 );

        const uint8_t * value = getGamePalette();

        for ( uint32_t i = 0; i < 256; ++i ) {
            const uint32_t red = static_cast<uint32_t>( *value ) * alpha / 255;
            ++value;
            const uint32_t green = static_cast<uint32_t>( *value ) * alpha / 255;
            ++value;
            const uint32_t blue = static_cast<uint32_t>( *value ) * alpha / 255;
            ++value;
            palette[i] = GetPALColorId( static_cast<uint8_t>( red ), static_cast<uint8_t>( green ), static_cast<uint8_t>( blue ) );
        }

        ApplyPalette( in, inX, inY, out, outX, outY, width, height, palette );
    }

    void ApplyTransform( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t transformId )
    {
        if ( !Verify( image, x, y, width, height ) ) {
            return;
        }

        // RGBA target: shadow transforms (2-5) encoded at the channel level for the GPU
        // shader to apply. mask=255 → palette-quantised remap via transformTable;
        // mask=0 → store transform id in idx as the shadow flag.
        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            if ( transformId < 2 || transformId > 5 ) {
                return;
            }
            uint8_t * idxBase = image.indexedBuffer();
            uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = image.indexedStride();
                for ( int32_t ry = 0; ry < height; ++ry ) {
                    for ( int32_t rx = 0; rx < width; ++rx ) {
                        const ptrdiff_t idxOff = static_cast<ptrdiff_t>( y + ry ) * idxStride + ( x + rx );
                        if ( maskBase[idxOff] != 0 ) {
                            idxBase[idxOff] = transformTable[static_cast<size_t>( transformId ) * 256 + idxBase[idxOff]];
                        }
                        else {
                            idxBase[idxOff] = transformId;
                        }
                    }
                }
                image.markIndexedDirty( { x, y, width, height } );
                return;
            }

            const float f = shadowFactor[transformId];
            const float scale = image.physicalScale();
            const int32_t bufStride = image.bufferStride();
            const int32_t bufHeight = image.bufferHeight();
            uint8_t * outBase = image.image();
            for ( int32_t ry = 0; ry < height; ++ry ) {
                for ( int32_t rx = 0; rx < width; ++rx ) {
                    const PhysicalBlock pb = toPhysicalBlock( x + rx, y + ry, scale, bufStride, bufHeight );
                    shadeRGBABlock( outBase, pb, bufStride, f );
                }
            }
            return;
        }

        const int32_t imageWidth = image.width();
        uint8_t * imageY = image.image() + y * imageWidth + x;
        const uint8_t * imageYEnd = imageY + height * imageWidth;

        if ( image.singleLayer() ) {
            for ( ; imageY != imageYEnd; imageY += imageWidth ) {
                uint8_t * imageX = imageY;
                const uint8_t * imageXEnd = imageX + width;
                for ( ; imageX != imageXEnd; ++imageX ) {
                    *imageX = *( transformTable + transformId * 256 + *imageX );
                }
            }
        }
        else {
            const uint8_t * transformY = image.transform() + y * imageWidth + x;
            for ( ; imageY != imageYEnd; imageY += imageWidth, transformY += imageWidth ) {
                uint8_t * imageX = imageY;
                const uint8_t * transformX = transformY;
                const uint8_t * imageXEnd = imageX + width;
                for ( ; imageX != imageXEnd; ++imageX, ++transformX ) {
                    if ( *transformX == 0 ) {
                        *imageX = *( transformTable + transformId * 256 + *imageX );
                    }
                }
            }
        }
    }

    void Blit( const Image & in, Image & out, const bool flip /* = false */ )
    {
        Blit( in, 0, 0, out, 0, 0, in.width(), in.height(), flip );
    }

    void Blit( const Image & in, Image & out, const Rect & outRoi, const bool flip /* = false */ )
    {
        Blit( in, 0, 0, out, outRoi.x, outRoi.y, outRoi.width, outRoi.height, flip );
    }

    void Blit( const Image & in, Image & out, const int32_t outX, const int32_t outY, const bool flip /* = false */ )
    {
        Blit( in, 0, 0, out, outX, outY, in.width(), in.height(), flip );
    }

    void Blit( const Image & in, const Point & inPos, Image & out, const Point & outPos, const Size & size, const bool flip /* = false */ )
    {
        Blit( in, inPos.x, inPos.y, out, outPos.x, outPos.y, size.width, size.height, flip );
    }

    void Blit( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const bool flip /* = false */ )
    {
        if ( in.format() == ImageFormat::RGBA_32BIT ) {
            if ( out.format() == ImageFormat::RGBA_32BIT ) {
                BlitRGBAToRGBAOutput( in, inX, inY, out, outX, outY, width, height, flip );
            }
            else {
                BlitRGBAToIndexed( in, inX, inY, out, outX, outY, width, height, flip );
            }
            return;
        }

        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            BlitIndexedToRGBAOutput( in, inX, inY, out, outX, outY, width, height, flip );
            return;
        }

        if ( in.singleLayer() && !flip ) {
            Copy( in, inX, inY, out, outX, outY, width, height );
            return;
        }

        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();

        if ( flip ) {
            const int32_t offsetInY = inY * widthIn + widthIn - 1 - inX;
            const uint8_t * imageInY = in.image() + offsetInY;
            const uint8_t * transformInY = in.transform() + offsetInY;

            const int32_t offsetOutY = outY * widthOut + outX;
            uint8_t * imageOutY = out.image() + offsetOutY;
            const uint8_t * imageOutYEnd = imageOutY + height * widthOut;

            if ( out.singleLayer() ) {
                assert( !in.singleLayer() );
                for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;

                    for ( ; imageOutX != imageOutXEnd; --imageInX, --transformInX, ++imageOutX ) {
                        if ( *transformInX > 0 ) {
                            if ( *transformInX != 1 ) {
                                *imageOutX = *( transformTable + ( *transformInX ) * 256 + *imageOutX );
                            }
                        }
                        else {
                            *imageOutX = *imageInX;
                        }
                    }
                }
            }
            else {
                uint8_t * transformOutY = out.transform() + offsetOutY;

                for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut, transformOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * imageOutX = imageOutY;
                    uint8_t * transformOutX = transformOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;

                    for ( ; imageOutX != imageOutXEnd; --imageInX, --transformInX, ++imageOutX, ++transformOutX ) {
                        if ( *transformInX == 1 ) {
                            continue;
                        }

                        if ( *transformInX > 0 && *transformOutX == 0 ) {
                            *imageOutX = *( transformTable + ( *transformInX ) * 256 + *imageOutX );
                        }
                        else {
                            *transformOutX = *transformInX;
                            *imageOutX = *imageInX;
                        }
                    }
                }
            }
        }
        else {
            const int32_t offsetInY = inY * widthIn + inX;
            const uint8_t * imageInY = in.image() + offsetInY;
            const uint8_t * transformInY = in.transform() + offsetInY;

            const int32_t offsetOutY = outY * widthOut + outX;
            uint8_t * imageOutY = out.image() + offsetOutY;
            const uint8_t * imageInYEnd = imageInY + height * widthIn;

            if ( out.singleLayer() ) {
                assert( !in.singleLayer() );
                for ( ; imageInY != imageInYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageInXEnd = imageInX + width;

                    for ( ; imageInX != imageInXEnd; ++imageInX, ++transformInX, ++imageOutX ) {
                        if ( *transformInX > 0 ) {
                            if ( *transformInX != 1 ) {
                                *imageOutX = *( transformTable + ( *transformInX ) * 256 + *imageOutX );
                            }
                        }
                        else {
                            *imageOutX = *imageInX;
                        }
                    }
                }
            }
            else {
                uint8_t * transformOutY = out.transform() + offsetOutY;

                for ( ; imageInY != imageInYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut, transformOutY += widthOut ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * imageOutX = imageOutY;
                    uint8_t * transformOutX = transformOutY;
                    const uint8_t * imageInXEnd = imageInX + width;

                    for ( ; imageInX != imageInXEnd; ++imageInX, ++transformInX, ++imageOutX, ++transformOutX ) {
                        if ( *transformInX == 1 ) {
                            continue;
                        }

                        if ( *transformInX > 0 && *transformOutX == 0 ) {
                            *imageOutX = *( transformTable + ( *transformInX ) * 256 + *imageOutX );
                        }
                        else {
                            *transformOutX = *transformInX;
                            *imageOutX = *imageInX;
                        }
                    }
                }
            }
        }
    }

    void Copy( const Image & in, Image & out )
    {
        // Same-format same-size full copy goes via assignment for indexed double-layer; everything
        // else falls through to the rect copy below.
        if ( in.format() == out.format() && !out.singleLayer() && !in.singleLayer() && in.format() == ImageFormat::INDEXED_8BIT ) {
            out = in;
            return;
        }

        if ( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT ) {
            // If either side has a buffer that doesn't match its game dimensions (Display),
            // the Image::operator= shortcut would copy the wrong byte count or geometry. Fall
            // through to the rect path which handles physical strides correctly.
            if ( in.bufferStride() == in.width() && in.bufferHeight() == in.height() && out.bufferStride() == out.width() && out.bufferHeight() == out.height() ) {
                out = in;
                return;
            }
            CopyRGBAToRGBAOutput( in, 0, 0, out, 0, 0, in.width(), in.height() );
            return;
        }

        const int32_t width = in.width();
        const int32_t height = in.height();

        out.resize( width, height );

        const size_t size = static_cast<size_t>( width ) * height;
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            CopyIndexedToRGBAOutput( in, 0, 0, out, 0, 0, width, height );
            return;
        }
        if ( out.singleLayer() ) {
            memcpy( out.image(), in.image(), size );
        }
        else {
            assert( in.singleLayer() );
            memcpy( out.image(), in.image(), size );
            memset( out.transform(), static_cast<uint8_t>( 0 ), size );
        }
    }

    void Copy( const Image & in, const int32_t inX, const int32_t inY, Image & out, const Rect & outRoi )
    {
        Copy( in, inX, inY, out, outRoi.x, outRoi.y, outRoi.width, outRoi.height );
    }

    void Copy( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height )
    {
        if ( in.format() == ImageFormat::INDEXED_8BIT && out.format() == ImageFormat::RGBA_32BIT ) {
            CopyIndexedToRGBAOutput( in, inX, inY, out, outX, outY, width, height );
            return;
        }

        if ( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT ) {
            CopyRGBAToRGBAOutput( in, inX, inY, out, outX, outY, width, height );
            return;
        }

        if ( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::INDEXED_8BIT ) {
            // Quantize.
            BlitRGBAToIndexed( in, inX, inY, out, outX, outY, width, height, false );
            return;
        }

        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();

        if ( inX == 0 && inY == 0 && outX == 0 && outY == 0 && width == widthIn && width == widthOut && height == in.height() && height == out.height() ) {
            Copy( in, out );
            return;
        }

        const int32_t offsetInY = inY * widthIn + inX;
        const uint8_t * imageInY = in.image() + offsetInY;

        const int32_t offsetOutY = outY * widthOut + outX;
        uint8_t * imageOutY = out.image() + offsetOutY;
        const uint8_t * imageOutYEnd = imageOutY + height * widthOut;

        if ( out.singleLayer() ) {
            for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, imageOutY += widthOut ) {
                memcpy( imageOutY, imageInY, static_cast<size_t>( width ) );
            }
        }
        else if ( in.singleLayer() ) {
            uint8_t * transformOutY = out.transform() + offsetOutY;

            for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, imageOutY += widthOut, transformOutY += widthOut ) {
                memcpy( imageOutY, imageInY, static_cast<size_t>( width ) );
                memset( transformOutY, static_cast<uint8_t>( 0 ), width );
            }
        }
        else {
            const uint8_t * transformInY = in.transform() + offsetInY;
            uint8_t * transformOutY = out.transform() + offsetOutY;

            for ( ; imageOutY != imageOutYEnd; imageInY += widthIn, transformInY += widthIn, imageOutY += widthOut, transformOutY += widthOut ) {
                memcpy( imageOutY, imageInY, static_cast<size_t>( width ) );
                memcpy( transformOutY, transformInY, static_cast<size_t>( width ) );
            }
        }
    }

    void copyTransformLayer( const Image & in, Image & out )
    {
        if ( in.empty() || out.empty() || in.singleLayer() || in.width() != out.width() || in.height() != out.height() ) {
            assert( 0 );
            return;
        }

        if ( out.singleLayer() ) {
            Image temp;
            Copy( out, temp );
            out = std::move( temp );
        }

        memcpy( out.transform(), in.transform(), in.width() * in.height() );
    }

    void copyTransformLayer( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height )
    {
        if ( in.empty() || out.empty() || in.singleLayer() ) {
            assert( 0 );
            return;
        }

        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        if ( out.singleLayer() ) {
            Image temp;
            Copy( out, temp );
            out = std::move( temp );
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();

        if ( inX == 0 && inY == 0 && outX == 0 && outY == 0 && width == widthIn && width == widthOut && height == in.height() && height == out.height() ) {
            copyTransformLayer( in, out );
            return;
        }

        const int32_t offsetInY = inY * widthIn + inX;
        const int32_t offsetOutY = outY * widthOut + outX;

        const uint8_t * transformInY = in.transform() + offsetInY;
        uint8_t * transformOutY = out.transform() + offsetOutY;
        const uint8_t * transformInYEnd = transformInY + static_cast<ptrdiff_t>( height ) * widthIn;

        for ( ; transformInY != transformInYEnd; transformInY += widthIn, transformOutY += widthOut ) {
            memcpy( transformOutY, transformInY, static_cast<size_t>( width ) );
        }
    }

    Sprite CreateContour( const Image & image, const uint8_t value )
    {
        if ( image.empty() || image.singleLayer() ) {
            assert( 0 );
            return {};
        }

        const int32_t width = image.width();
        const int32_t height = image.height();

        Sprite contour( width, height );
        contour.reset();
        if ( width < 2 || height < 2 ) {
            return contour;
        }

        assert( !contour.empty() );

        const uint8_t * inY = image.transform();
        uint8_t * outImageY = contour.image();
        uint8_t * outTransformY = contour.transform();

        const int32_t reducedWidth = width - 1;
        const int32_t reducedHeight = height - 1;

        for ( int32_t y = 0; y < height; ++y, inY += width, outImageY += width, outTransformY += width ) {
            const uint8_t * inX = inY;

            const bool isNotTopRow = ( y > 0 );
            const bool isNotBottomRow = ( y < reducedHeight );

            for ( int32_t x = 0; x < width; ++x, ++inX ) {
                if ( *inX > 0 && *inX < 6
                     && ( ( x > 0 && *( inX - 1 ) == 0 ) || ( x < reducedWidth && *( inX + 1 ) == 0 ) || ( isNotTopRow && *( inX - width ) == 0 )
                          || ( isNotBottomRow && *( inX + width ) == 0 ) ) ) {
                    outImageY[x] = value;
                    outTransformY[x] = 0;
                }
            }
        }

        return contour;
    }

    void CreateDitheringTransition( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                    const bool isVertical, const bool isReverse )
    {
        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        // RGBA-output: do per-pixel selection using the same pattern but write RGBA.
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            // Lazy port: just Copy the relevant pixels for now (no dither). This degrades the
            // visual transition but avoids the full re-write; CreateDitheringTransition is only
            // used in a few places (scenario fade-in, etc).
            Copy( in, inX, inY, out, outX, outY, width, height );
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t offsetIn = inY * widthIn + inX;
        const uint8_t * imageIn = in.image() + offsetIn;
        const uint8_t * transformIn = in.singleLayer() ? nullptr : in.transform() + offsetIn;

        const int32_t widthOut = out.width();
        const int32_t offsetOut = outY * widthOut + outX;
        uint8_t * imageOut = out.image() + offsetOut;
        uint8_t * transformOut = out.singleLayer() ? nullptr : out.transform() + offsetOut;

        if ( isVertical ) {
            const uint8_t * imageInRightPoint = imageIn + width - 1;
            uint8_t * imageOutRightPoint = imageOut + width - 1;
            const uint8_t * transformInRightPoint = ( transformIn == nullptr ) ? nullptr : transformIn + width - 1;
            uint8_t * transformOutRightPoint = ( transformOut == nullptr ) ? nullptr : transformOut + width - 1;

            const int32_t halfWidth = width / 2;

            if ( ( width % 2 ) == 1 ) {
                if ( isReverse ) {
                    --imageOutRightPoint;
                    --imageInRightPoint;
                    if ( transformOutRightPoint != nullptr ) {
                        --transformOutRightPoint;
                    }
                    if ( transformInRightPoint != nullptr ) {
                        --transformInRightPoint;
                    }
                }
                else {
                    ++imageOut;
                    ++imageIn;
                    if ( transformOut != nullptr ) {
                        ++transformOut;
                    }
                    if ( transformIn != nullptr ) {
                        ++transformIn;
                    }
                }
            }

            for ( int32_t x = 0; x < halfWidth; ++x ) {
                const int32_t stepPower = std::min<int32_t>( 30, ( halfWidth - x ) / 2 + 1 );
                const int32_t stepY = 1 << stepPower;
                const int32_t patternPoint = stepY / 2 * ( ( x + halfWidth ) % 2 );

                for ( int32_t y = 0; y < height; ++y ) {
                    const int32_t offsetOutX = y * widthOut;
                    const int32_t offsetInX = y * widthIn;
                    const int32_t offsetY = y % stepY;

                    if ( isReverse == ( patternPoint != offsetY ) ) {
                        if ( transformIn != nullptr && transformOut == nullptr && ( *( transformIn + offsetInX ) == 1 ) ) {
                            continue;
                        }
                        *( imageOut + offsetOutX ) = *( imageIn + offsetInX );
                        if ( transformOut == nullptr ) {
                            continue;
                        }
                        if ( transformIn == nullptr ) {
                            *( transformOut + offsetOutX ) = 0;
                        }
                        else {
                            *( transformOut + offsetOutX ) = *( transformIn + offsetInX );
                        }
                    }
                    else {
                        if ( transformInRightPoint != nullptr && transformOutRightPoint == nullptr && ( *( transformInRightPoint + offsetInX ) == 1 ) ) {
                            continue;
                        }
                        *( imageOutRightPoint + offsetOutX ) = *( imageInRightPoint + offsetInX );
                        if ( transformOutRightPoint == nullptr ) {
                            continue;
                        }
                        if ( transformInRightPoint == nullptr ) {
                            *( transformOutRightPoint + offsetOutX ) = 0;
                        }
                        else {
                            *( transformOutRightPoint + offsetOutX ) = *( transformInRightPoint + offsetInX );
                        }
                    }
                }

                ++imageOut;
                --imageOutRightPoint;
                ++imageIn;
                --imageInRightPoint;

                if ( transformOut != nullptr ) {
                    ++transformOut;
                    --transformOutRightPoint;
                }
                if ( transformIn != nullptr ) {
                    ++transformIn;
                    --transformInRightPoint;
                }
            }
        }
        else {
            const int32_t offsetInBottomOffset = ( height - 1 ) * widthIn;
            const int32_t offsetOutYBottomOffset = ( height - 1 ) * widthOut;
            const uint8_t * imageInBottomPoint = imageIn + offsetInBottomOffset;
            uint8_t * imageOutBottomPoint = imageOut + offsetOutYBottomOffset;

            const uint8_t * transformInBottomPoint = ( transformIn == nullptr ) ? nullptr : transformIn + offsetInBottomOffset;
            uint8_t * transformOutBottomPoint = ( transformOut == nullptr ) ? nullptr : transformOut + offsetOutYBottomOffset;

            const int32_t halfHeight = height / 2;

            if ( ( height % 2 ) == 1 ) {
                if ( isReverse ) {
                    imageOutBottomPoint -= widthOut;
                    imageInBottomPoint -= widthIn;
                    if ( transformOutBottomPoint != nullptr ) {
                        transformOutBottomPoint -= widthOut;
                    }
                    if ( transformInBottomPoint != nullptr ) {
                        transformInBottomPoint -= widthIn;
                    }
                }
                else {
                    imageOut += widthOut;
                    imageIn += widthIn;
                    if ( transformOut != nullptr ) {
                        transformOut += widthOut;
                    }
                    if ( transformIn != nullptr ) {
                        transformIn += widthIn;
                    }
                }
            }

            for ( int32_t y = 0; y < halfHeight; ++y ) {
                const int32_t stepPower = std::min<int32_t>( 30, ( halfHeight - y ) / 2 + 1 );
                const int32_t stepX = 1 << stepPower;
                const int32_t patternPoint = stepX / 2 * ( ( y + halfHeight ) % 2 );

                for ( int32_t x = 0; x < width; ++x ) {
                    const int32_t offsetX = x % stepX;

                    if ( isReverse == ( patternPoint != offsetX ) ) {
                        if ( transformIn != nullptr && transformOut == nullptr && ( *( transformIn + x ) == 1 ) ) {
                            continue;
                        }
                        *( imageOut + x ) = *( imageIn + x );
                        if ( transformOut == nullptr ) {
                            continue;
                        }
                        if ( transformIn == nullptr ) {
                            *( transformOut + x ) = 0;
                        }
                        else {
                            *( transformOut + x ) = *( transformIn + x );
                        }
                    }
                    else {
                        if ( transformInBottomPoint != nullptr && transformOutBottomPoint == nullptr && ( *( transformInBottomPoint + x ) == 1 ) ) {
                            continue;
                        }
                        *( imageOutBottomPoint + x ) = *( imageInBottomPoint + x );
                        if ( transformOutBottomPoint == nullptr ) {
                            continue;
                        }
                        if ( transformInBottomPoint == nullptr ) {
                            *( transformOutBottomPoint + x ) = 0;
                        }
                        else {
                            *( transformOutBottomPoint + x ) = *( transformInBottomPoint + x );
                        }
                    }
                }

                imageOut += widthOut;
                imageOutBottomPoint -= widthOut;
                imageIn += widthIn;
                imageInBottomPoint -= widthIn;

                if ( transformOut != nullptr ) {
                    transformOut += widthOut;
                    transformOutBottomPoint -= widthOut;
                }
                if ( transformIn != nullptr ) {
                    transformIn += widthIn;
                    transformInBottomPoint -= widthIn;
                }
            }
        }
    }

    Sprite Crop( const Image & image, int32_t x, int32_t y, int32_t width, int32_t height )
    {
        if ( image.empty() || width <= 0 || height <= 0 ) {
            return {};
        }

        if ( x < 0 ) {
            const int32_t offsetX = -x;
            if ( offsetX >= width ) {
                return {};
            }
            x = 0;
            width -= offsetX;
        }

        if ( y < 0 ) {
            const int32_t offsetY = -y;
            if ( offsetY >= height ) {
                return {};
            }
            y = 0;
            height -= offsetY;
        }

        if ( x > image.width() || y > image.height() ) {
            return {};
        }

        if ( x + width > image.width() ) {
            const int32_t offsetX = x + width - image.width();
            width -= offsetX;
        }

        if ( y + height > image.height() ) {
            const int32_t offsetY = y + height - image.height();
            height -= offsetY;
        }

        Sprite out;
        if ( image.singleLayer() ) {
            out._disableTransformLayer();
        }
        out.resize( width, height );

        Copy( image, x, y, out, 0, 0, width, height );
        out.setPosition( x, y );
        return out;
    }

    void DrawBorder( Image & image, const uint8_t value, const uint32_t skipFactor /* =0 */ )
    {
        if ( image.empty() || image.width() < 2 || image.height() < 2 ) {
            return;
        }

        const int32_t width = image.width();
        const int32_t height = image.height();

        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            // Phase 3 Display fast path: indexed/mask one-byte writes per game pixel.
            uint8_t * idxBase = image.indexedBuffer();
            uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = image.indexedStride();
                const auto setIdxPixel = [&]( int32_t px, int32_t py ) {
                    const ptrdiff_t off = static_cast<ptrdiff_t>( py ) * idxStride + px;
                    idxBase[off] = value;
                    maskBase[off] = 255;
                };

                uint32_t counter = 1;
                for ( int32_t x = 0; x < width; ++x ) {
                    if ( skipFactor < 2 || counter % skipFactor != 0 ) {
                        setIdxPixel( x, 0 );
                        setIdxPixel( x, height - 1 );
                    }
                    ++counter;
                }
                for ( int32_t y = 1; y < height - 1; ++y ) {
                    if ( skipFactor < 2 || counter % skipFactor != 0 ) {
                        setIdxPixel( 0, y );
                        setIdxPixel( width - 1, y );
                    }
                    ++counter;
                }
                image.markIndexedDirty( { 0, 0, width, height } );
                return;
            }

            uint8_t r;
            uint8_t g;
            uint8_t b;
            paletteIdxToRGBA( value, r, g, b );

            const float scale = image.physicalScale();
            const int32_t bufStride = image.bufferStride();
            const int32_t bufHeight = image.bufferHeight();
            uint8_t * outBase = image.image();

            const auto setPixel = [&]( int32_t px, int32_t py ) {
                const PhysicalBlock pb = toPhysicalBlock( px, py, scale, bufStride, bufHeight );
                fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
            };

            uint32_t counter = 1;
            for ( int32_t x = 0; x < width; ++x ) {
                if ( skipFactor < 2 || counter % skipFactor != 0 ) {
                    setPixel( x, 0 );
                    setPixel( x, height - 1 );
                }
                ++counter;
            }
            for ( int32_t y = 1; y < height - 1; ++y ) {
                if ( skipFactor < 2 || counter % skipFactor != 0 ) {
                    setPixel( 0, y );
                    setPixel( width - 1, y );
                }
                ++counter;
            }
            if ( idxBase != nullptr ) {
                clearIndexedBboxOnDisplay( image, 0, 0, width, height );
            }
            return;
        }

        uint8_t * dataPointer = image.image();
        uint8_t * transformPointer = image.singleLayer() ? nullptr : image.transform();

        if ( skipFactor < 2 ) {
            // top side
            uint8_t * data = dataPointer;
            const uint8_t * dataEnd = data + width;
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; ++data ) {
                    *data = value;
                }
            }
            else {
                uint8_t * transform = transformPointer;
                for ( ; data != dataEnd; ++data, ++transform ) {
                    *data = value;
                    *transform = 0;
                }
            }

            // bottom side
            data = dataPointer + width * static_cast<ptrdiff_t>( height - 1 );
            dataEnd = data + width;
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; ++data ) {
                    *data = value;
                }
            }
            else {
                uint8_t * transform = transformPointer + width * static_cast<ptrdiff_t>( height - 1 );
                for ( ; data != dataEnd; ++data, ++transform ) {
                    *data = value;
                    *transform = 0;
                }
            }

            // left side
            data = dataPointer + width;
            dataEnd = data + width * ( height - 2 );
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; data += width ) {
                    *data = value;
                }
            }
            else {
                uint8_t * transform = transformPointer + width;
                for ( ; data != dataEnd; data += width, transform += width ) {
                    *data = value;
                    *transform = 0;
                }
            }

            // right side
            data = dataPointer + width + width - 1;
            dataEnd = data + width * ( height - 2 );
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; data += width ) {
                    *data = value;
                }
            }
            else {
                uint8_t * transform = transformPointer + width + width - 1;
                for ( ; data != dataEnd; data += width, transform += width ) {
                    *data = value;
                    *transform = 0;
                }
            }
        }
        else {
            uint32_t counter = 1;

            uint8_t * data = dataPointer;
            const uint8_t * dataEnd = data + width;
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; ++data ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                    }
                    ++counter;
                }
            }
            else {
                uint8_t * transform = transformPointer;
                for ( ; data != dataEnd; ++data, ++transform ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                        *transform = 0;
                    }
                    ++counter;
                }
            }

            data = dataPointer + width + width - 1;
            dataEnd = data + width * ( height - 2 );
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; data += width ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                    }
                    ++counter;
                }
            }
            else {
                uint8_t * transform = transformPointer + width + width - 1;
                for ( ; data != dataEnd; data += width, transform += width ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                        *transform = 0;
                    }
                    ++counter;
                }
            }

            data = dataPointer + width * static_cast<ptrdiff_t>( height - 1 ) + width - 1;
            dataEnd = data - width;
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; --data ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                    }
                    ++counter;
                }
            }
            else {
                uint8_t * transform = transformPointer + width * static_cast<ptrdiff_t>( height - 1 ) + width - 1;
                for ( ; data != dataEnd; --data, --transform ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                        *transform = 0;
                    }
                    ++counter;
                }
            }

            data = dataPointer + width * static_cast<ptrdiff_t>( height - 2 );
            dataEnd = dataPointer;
            if ( transformPointer == nullptr ) {
                for ( ; data != dataEnd; data -= width ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                    }
                    ++counter;
                }
            }
            else {
                uint8_t * transform = transformPointer + width * static_cast<ptrdiff_t>( height - 2 );
                for ( ; data != dataEnd; data -= width, transform -= width ) {
                    if ( counter % skipFactor != 0 ) {
                        *data = value;
                        *transform = 0;
                    }
                    ++counter;
                }
            }
        }
    }

    void DrawLine( Image & image, const Point & start, const Point & end, const uint8_t value, const Rect & roi /* = Rect() */ )
    {
        if ( image.empty() ) {
            return;
        }

        const int32_t width = image.width();
        const int32_t height = image.height();

        int32_t x1 = start.x;
        int32_t y1 = start.y;
        const int32_t x2 = end.x;
        const int32_t y2 = end.y;

        const int32_t dx = std::abs( x2 - x1 );
        const int32_t dy = std::abs( y2 - y1 );

        const bool isValidRoi = roi.width > 0 && roi.height > 0;

        const int32_t minX = isValidRoi ? std::max<int32_t>( roi.x, 0 ) : 0;
        const int32_t minY = isValidRoi ? std::max<int32_t>( roi.y, 0 ) : 0;
        int32_t maxX = isValidRoi ? roi.x + roi.width : width;
        int32_t maxY = isValidRoi ? roi.y + roi.height : height;

        if ( minX >= width || minY >= height ) {
            return;
        }

        if ( maxX >= width ) {
            maxX = width;
        }
        if ( maxY >= height ) {
            maxY = height;
        }

        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            // Phase 3 Display fast path: write the index byte + mask per game pixel.
            uint8_t * idxBase = image.indexedBuffer();
            uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = image.indexedStride();
                const auto plotIdx = [&]( int32_t px, int32_t py ) {
                    if ( px < minX || px >= maxX || py < minY || py >= maxY ) {
                        return;
                    }
                    const ptrdiff_t off = static_cast<ptrdiff_t>( py ) * idxStride + px;
                    idxBase[off] = value;
                    maskBase[off] = 255;
                };
                int32_t lx1 = x1;
                int32_t ly1 = y1;
                if ( dx >= dy ) {
                    int32_t ns = dx / 2;
                    for ( int32_t i = 0; i <= dx; ++i ) {
                        plotIdx( lx1, ly1 );
                        lx1 < x2 ? ++lx1 : --lx1;
                        ns -= dy;
                        if ( ns < 0 ) {
                            ly1 < y2 ? ++ly1 : --ly1;
                            ns += dx;
                        }
                    }
                }
                else {
                    int32_t ns = dy / 2;
                    for ( int32_t i = 0; i <= dy; ++i ) {
                        plotIdx( lx1, ly1 );
                        ly1 < y2 ? ++ly1 : --ly1;
                        ns -= dx;
                        if ( ns < 0 ) {
                            lx1 < x2 ? ++lx1 : --lx1;
                            ns += dy;
                        }
                    }
                }
                const int32_t lx = std::min( start.x, end.x );
                const int32_t ly = std::min( start.y, end.y );
                const int32_t lw = std::abs( end.x - start.x ) + 1;
                const int32_t lh = std::abs( end.y - start.y ) + 1;
                image.markIndexedDirty( { lx, ly, lw, lh } );
                return;
            }

            uint8_t r;
            uint8_t g;
            uint8_t b;
            paletteIdxToRGBA( value, r, g, b );

            const float scale = image.physicalScale();
            const int32_t bufStride = image.bufferStride();
            const int32_t bufHeight = image.bufferHeight();
            uint8_t * outBase = image.image();

            const auto plot = [&]( int32_t px, int32_t py ) {
                if ( px < minX || px >= maxX || py < minY || py >= maxY ) {
                    return;
                }
                const PhysicalBlock pb = toPhysicalBlock( px, py, scale, bufStride, bufHeight );
                fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
            };

            if ( dx >= dy ) {
                int32_t ns = dx / 2;
                for ( int32_t i = 0; i <= dx; ++i ) {
                    plot( x1, y1 );
                    x1 < x2 ? ++x1 : --x1;
                    ns -= dy;
                    if ( ns < 0 ) {
                        y1 < y2 ? ++y1 : --y1;
                        ns += dx;
                    }
                }
            }
            else {
                int32_t ns = dy / 2;
                for ( int32_t i = 0; i <= dy; ++i ) {
                    plot( x1, y1 );
                    y1 < y2 ? ++y1 : --y1;
                    ns -= dx;
                    if ( ns < 0 ) {
                        x1 < x2 ? ++x1 : --x1;
                        ns += dy;
                    }
                }
            }
            if ( idxBase != nullptr ) {
                const int32_t lx = std::min( start.x, end.x );
                const int32_t ly = std::min( start.y, end.y );
                const int32_t lw = std::abs( end.x - start.x ) + 1;
                const int32_t lh = std::abs( end.y - start.y ) + 1;
                clearIndexedBboxOnDisplay( image, lx, ly, lw, lh );
            }
            return;
        }

        uint8_t * data = image.image();

        if ( image.singleLayer() ) {
            if ( dx >= dy ) {
                int32_t ns = dx / 2;
                for ( int32_t i = 0; i <= dx; ++i ) {
                    if ( x1 >= minX && x1 < maxX && y1 >= minY && y1 < maxY ) {
                        const int32_t offset = x1 + y1 * width;
                        *( data + offset ) = value;
                    }
                    x1 < x2 ? ++x1 : --x1;
                    ns -= dy;
                    if ( ns < 0 ) {
                        y1 < y2 ? ++y1 : --y1;
                        ns += dx;
                    }
                }
            }
            else {
                int32_t ns = dy / 2;
                for ( int32_t i = 0; i <= dy; ++i ) {
                    if ( x1 >= minX && x1 < maxX && y1 >= minY && y1 < maxY ) {
                        const int32_t offset = x1 + y1 * width;
                        *( data + offset ) = value;
                    }
                    y1 < y2 ? ++y1 : --y1;
                    ns -= dx;
                    if ( ns < 0 ) {
                        x1 < x2 ? ++x1 : --x1;
                        ns += dy;
                    }
                }
            }
        }
        else {
            uint8_t * transform = image.transform();

            if ( dx >= dy ) {
                int32_t ns = dx / 2;
                for ( int32_t i = 0; i <= dx; ++i ) {
                    if ( x1 >= minX && x1 < maxX && y1 >= minY && y1 < maxY ) {
                        const int32_t offset = x1 + y1 * width;
                        *( data + offset ) = value;
                        *( transform + offset ) = 0;
                    }
                    x1 < x2 ? ++x1 : --x1;
                    ns -= dy;
                    if ( ns < 0 ) {
                        y1 < y2 ? ++y1 : --y1;
                        ns += dx;
                    }
                }
            }
            else {
                int32_t ns = dy / 2;
                for ( int32_t i = 0; i <= dy; ++i ) {
                    if ( x1 >= minX && x1 < maxX && y1 >= minY && y1 < maxY ) {
                        const int32_t offset = x1 + y1 * width;
                        *( data + offset ) = value;
                        *( transform + offset ) = 0;
                    }
                    y1 < y2 ? ++y1 : --y1;
                    ns -= dx;
                    if ( ns < 0 ) {
                        x1 < x2 ? ++x1 : --x1;
                        ns += dy;
                    }
                }
            }
        }
    }

    void DrawRect( Image & image, const Rect & roi, const uint8_t value )
    {
        if ( image.empty() || roi.width < 1 || roi.height < 1 ) {
            return;
        }

        DrawLine( image, { roi.x, roi.y }, { roi.x + roi.width, roi.y }, value, roi );
        DrawLine( image, { roi.x, roi.y }, { roi.x, roi.y + roi.height }, value, roi );
        DrawLine( image, { roi.x + roi.width - 1, roi.y }, { roi.x + roi.width - 1, roi.y + roi.height }, value, roi );
        DrawLine( image, { roi.x, roi.y + roi.height - 1 }, { roi.x + roi.width, roi.y + roi.height - 1 }, value, roi );
    }

    void DivideImageBySquares( const Point & spriteOffset, const Image & original, const int32_t squareSize, std::vector<Point> & outputSquareId,
                               std::vector<std::pair<Point, Rect>> & outputImageInfo )
    {
        if ( original.empty() ) {
            return;
        }

        if ( squareSize <= 0 ) {
            assert( 0 );
            return;
        }

        Point offset{ spriteOffset.x / squareSize, spriteOffset.y / squareSize };

        if ( ( spriteOffset.x < 0 ) && ( offset.x * squareSize != spriteOffset.x ) ) {
            --offset.x;
        }

        if ( ( spriteOffset.y < 0 ) && ( offset.y * squareSize != spriteOffset.y ) ) {
            --offset.y;
        }

        const Point spriteRelativeOffset{ spriteOffset.x - offset.x * squareSize, spriteOffset.y - offset.y * squareSize };
        const Point stepPerDirection{ ( original.width() + spriteRelativeOffset.x + squareSize - 1 ) / squareSize,
                                      ( original.height() + spriteRelativeOffset.y + squareSize - 1 ) / squareSize };
        assert( stepPerDirection.x > 0 && stepPerDirection.y > 0 );

        const Rect relativeROI( spriteRelativeOffset.x, spriteRelativeOffset.y, original.width(), original.height() );

        for ( int32_t y = 0; y < stepPerDirection.y; ++y ) {
            for ( int32_t x = 0; x < stepPerDirection.x; ++x ) {
                const Rect roi( x * squareSize, y * squareSize, squareSize, squareSize );
                const Rect intersection = relativeROI ^ roi;
                assert( intersection.width > 0 && intersection.height > 0 );

                outputSquareId.emplace_back( offset + Point( x, y ) );

                outputImageInfo.emplace_back( fheroes2::Point( intersection.x - roi.x, intersection.y - roi.y ),
                                              fheroes2::Rect( intersection.x - spriteRelativeOffset.x, intersection.y - spriteRelativeOffset.y, intersection.width,
                                                              intersection.height ) );
            }
        }
    }

    Image ExtractCommonPattern( const std::vector<const Image *> & input )
    {
        if ( input.empty() ) {
            return {};
        }

        assert( input.front() != nullptr );

        if ( input.size() == 1 ) {
            return *input.front();
        }

        if ( input.front()->empty() ) {
            return {};
        }

        const int32_t inputFrontWidth = input.front()->width();
        const int32_t inputFrontHeight = input.front()->height();

        for ( const Image * image : input ) {
            assert( image != nullptr );
            if ( image->width() != inputFrontWidth || image->height() != inputFrontHeight ) {
                return {};
            }
        }

        std::vector<const uint8_t *> imageIn( input.size() );
        std::vector<const uint8_t *> transformIn( input.size() );

        for ( size_t i = 0; i < input.size(); ++i ) {
            imageIn[i] = input[i]->image();

            transformIn[i] = input[i]->singleLayer() ? nullptr : input[i]->transform();
        }

        Image out( inputFrontWidth, inputFrontHeight );
        out.reset();

        uint8_t * imageOut = out.image();
        uint8_t * transformOut = out.transform();
        const uint8_t * imageOutEnd = imageOut + out.width() * out.height();

        bool isEqual = false;

        for ( ; imageOut != imageOutEnd; ++imageOut, ++transformOut ) {
            isEqual = true;

            for ( size_t i = 1; i < input.size(); ++i ) {
                if ( *imageIn[0] != *imageIn[i] || ( transformIn[0] != nullptr && transformIn[i] != nullptr && ( *transformIn[0] != *transformIn[i] ) ) ) {
                    isEqual = false;
                    break;
                }
            }

            if ( isEqual ) {
                *imageOut = *imageIn[0];
                *transformOut = ( transformIn[0] == nullptr ) ? 0 : *transformIn[0];
            }

            for ( size_t i = 0; i < input.size(); ++i ) {
                ++imageIn[i];
                if ( transformIn[i] != nullptr ) {
                    ++transformIn[i];
                }
            }
        }

        return out;
    }

    void Fill( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t colorId )
    {
        if ( !Verify( image, x, y, width, height ) ) {
            return;
        }

        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            // Phase 3 Display fast path: memset both indexed and mask buffers per row.
            // The mask channel carries the validity bit so any colorId 0..255 is
            // representable (palette[0] is a real colour and Fill(0) should show it).
            uint8_t * idxBase = image.indexedBuffer();
            uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = image.indexedStride();
                for ( int32_t row = 0; row < height; ++row ) {
                    const ptrdiff_t off = static_cast<ptrdiff_t>( y + row ) * idxStride + x;
                    std::memset( idxBase + off, colorId, static_cast<size_t>( width ) );
                    std::memset( maskBase + off, 255, static_cast<size_t>( width ) );
                }
                image.markIndexedDirty( { x, y, width, height } );
                return;
            }

            uint8_t r;
            uint8_t g;
            uint8_t b;
            paletteIdxToRGBA( colorId, r, g, b );

            const float scale = image.physicalScale();
            const int32_t bufStride = image.bufferStride();
            const int32_t bufHeight = image.bufferHeight();
            uint8_t * outBase = image.image();

            const int32_t pXStart = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( x ) * scale ) );
            const int32_t pXEnd = std::min<int32_t>( bufStride, static_cast<int32_t>( static_cast<float>( x + width ) * scale ) );
            const int32_t pYStart = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( y ) * scale ) );
            const int32_t pYEnd = std::min<int32_t>( bufHeight, static_cast<int32_t>( static_cast<float>( y + height ) * scale ) );
            for ( int32_t py = pYStart; py < pYEnd; ++py ) {
                uint8_t * dstRow = outBase + ( static_cast<ptrdiff_t>( py ) * bufStride + pXStart ) * 4;
                for ( int32_t px = pXStart; px < pXEnd; ++px, dstRow += 4 ) {
                    dstRow[0] = r;
                    dstRow[1] = g;
                    dstRow[2] = b;
                    dstRow[3] = 255;
                }
            }
            return;
        }

        if ( image.width() == width && image.height() == height ) {
            image.fill( colorId );
            return;
        }

        const int32_t imageWidth = image.width();
        uint8_t * imageY = image.image() + y * imageWidth + x;
        const uint8_t * imageYEnd = imageY + height * imageWidth;

        if ( image.singleLayer() ) {
            for ( ; imageY != imageYEnd; imageY += imageWidth ) {
                memset( imageY, colorId, width );
            }
        }
        else {
            uint8_t * transformY = image.transform() + static_cast<ptrdiff_t>( y ) * imageWidth + x;
            for ( ; imageY != imageYEnd; imageY += imageWidth, transformY += imageWidth ) {
                memset( imageY, colorId, width );
                memset( transformY, static_cast<uint8_t>( 0 ), width );
            }
        }
    }

    void FillTransform( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t transformId )
    {
        if ( !Verify( image, x, y, width, height ) || image.singleLayer() ) {
            return;
        }

        const int32_t imageWidth = image.width();
        uint8_t * imageY = image.image() + y * imageWidth + x;
        uint8_t * transformY = image.transform() + y * imageWidth + x;
        const uint8_t * imageYEnd = imageY + height * imageWidth;

        for ( ; imageY != imageYEnd; imageY += imageWidth, transformY += imageWidth ) {
            memset( imageY, static_cast<uint8_t>( 0 ), width );
            memset( transformY, transformId, width );
        }
    }

    Image FilterOnePixelNoise( const Image & input )
    {
        if ( input.width() < 3 || input.height() < 3 ) {
            return input;
        }

        const int32_t width = input.width();
        const int32_t height = input.height();
        const bool isSingleLayer = input.singleLayer();

        Image output;
        if ( isSingleLayer ) {
            output._disableTransformLayer();
        }
        output.resize( width, height );
        output.reset();

        const uint8_t * imageInY = input.image();
        uint8_t * imageOutY = output.image();

        if ( isSingleLayer ) {
            for ( int32_t y = 0; y < height; ++y ) {
                for ( int32_t x = 0; x < width; ++x ) {
                    if ( ( x == 0 || x == width - 1 ) && ( y == 0 || y == height - 1 ) ) {
                        *( imageOutY + x ) = *( imageInY + x );
                    }
                }
                imageInY += width;
                imageOutY += width;
            }
        }
        else {
            const uint8_t * transformInY = input.transform();
            uint8_t * transformOutY = output.transform();

            for ( int32_t y = 0; y < height; ++y ) {
                const uint8_t * transformInX = transformInY;

                for ( int32_t x = 0; x < width; ++x ) {
                    if ( *transformInX == 0 && ( x == 0 || x == width - 1 || *( transformInX - 1 ) == 0 || *( transformInX + 1 ) == 0 )
                         && ( y == 0 || y == height - 1 || *( transformInX - width ) == 0 || *( transformInX + width ) == 0 ) ) {
                        *( transformOutY + x ) = 0;
                        *( imageOutY + x ) = *( imageInY + x );
                    }
                    ++transformInX;
                }

                imageInY += width;
                transformInY += width;
                imageOutY += width;
                transformOutY += width;
            }
        }

        return output;
    }

    bool FitToRoi( const Image & in, Point & inPos, const Image & out, Point & outPos, Size & outputSize, const Rect & outputRoi )
    {
        if ( !Validate( out, outputRoi.x, outputRoi.y, outputRoi.width, outputRoi.height ) ) {
            return false;
        }

        outPos.x -= outputRoi.x;
        outPos.y -= outputRoi.y;

        if ( !Verify( inPos.x, inPos.y, outPos.x, outPos.y, outputSize.width, outputSize.height, in.width(), in.height(), outputRoi.width, outputRoi.height ) ) {
            return false;
        }

        outPos.x += outputRoi.x;
        outPos.y += outputRoi.y;

        return true;
    }

    Image Flip( const Image & in, const bool horizontally, const bool vertically )
    {
        if ( in.empty() ) {
            return {};
        }

        const int32_t width = in.width();
        const int32_t height = in.height();

        Image out;
        if ( in.singleLayer() ) {
            out._disableTransformLayer();
        }
        out.resize( width, height );

        if ( !horizontally && !vertically ) {
            Copy( in, out );
            return out;
        }

        Flip( in, 0, 0, out, 0, 0, width, height, horizontally, vertically );
        return out;
    }

    void Flip( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const bool horizontally,
               const bool vertically )
    {
        if ( !Verify( in, inX, inY, out, outX, outY, width, height ) ) {
            return;
        }

        if ( !horizontally && !vertically ) {
            Copy( in, inX, inY, out, outX, outY, width, height );
            return;
        }

        // RGBA path: pixel-by-pixel write expanded to physical block on Display.
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();
            const uint8_t * inBase = in.image();
            for ( int32_t y = 0; y < height; ++y ) {
                const int32_t srcGameY = vertically ? ( inY + height - 1 - y ) : ( inY + y );
                const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( srcGameY ) * inScale );
                const uint8_t * srcRow = inBase + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;
                for ( int32_t x = 0; x < width; ++x ) {
                    const int32_t srcGameX = horizontally ? ( inX + width - 1 - x ) : ( inX + x );
                    const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcGameX ) * inScale );
                    const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                    const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                    fillRGBABlock( outBase, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcPx[3] );
                }
            }
            return;
        }

        assert( !in.empty() && !out.empty() );

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();
        const int32_t offsetOut = outY * widthOut + outX;
        const int32_t offsetIn = inY * widthIn + inX;

        uint8_t * imageOutY = out.image() + offsetOut;
        const uint8_t * imageOutYEnd = imageOutY + static_cast<ptrdiff_t>( widthOut ) * height;
        uint8_t * transformOutY = out.singleLayer() ? nullptr : out.transform() + offsetOut;

        if ( horizontally && !vertically ) {
            const uint8_t * imageInY = in.image() + offsetIn + width - 1;

            if ( in.singleLayer() ) {
                for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, imageInY += widthIn ) {
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;
                    const uint8_t * imageInX = imageInY;

                    for ( ; imageOutX != imageOutXEnd; ++imageOutX, --imageInX ) {
                        *imageOutX = *imageInX;
                    }

                    if ( transformOutY != nullptr ) {
                        memset( transformOutY, static_cast<uint8_t>( 0 ), width );
                        transformOutY += widthOut;
                    }
                }
            }
            else {
                const uint8_t * transformInY = in.transform() + offsetIn + width - 1;

                for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, imageInY += widthIn, transformInY += widthIn ) {
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;

                    if ( transformOutY == nullptr ) {
                        for ( ; imageOutX != imageOutXEnd; ++imageOutX, --imageInX, --transformInX ) {
                            if ( *transformInX == 0 ) {
                                *imageOutX = *imageInX;
                            }
                        }
                    }
                    else {
                        uint8_t * transformOutX = transformOutY;

                        for ( ; imageOutX != imageOutXEnd; ++imageOutX, ++transformOutX, --imageInX, --transformInX ) {
                            *imageOutX = *imageInX;
                            *transformOutX = *transformInX;
                        }
                        transformOutY += widthOut;
                    }
                }
            }
        }
        else if ( !horizontally && vertically ) {
            const uint8_t * imageInY = in.image() + offsetIn + static_cast<ptrdiff_t>( height - 1 ) * widthIn;

            if ( in.singleLayer() ) {
                for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, imageInY -= widthIn ) {
                    memcpy( imageOutY, imageInY, static_cast<size_t>( width ) );
                    if ( transformOutY != nullptr ) {
                        memset( transformOutY, static_cast<uint8_t>( 0 ), width );
                        transformOutY += widthOut;
                    }
                }
            }
            else {
                const uint8_t * transformInY = in.transform() + offsetIn + static_cast<ptrdiff_t>( height - 1 ) * widthIn;

                if ( transformOutY == nullptr ) {
                    for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, imageInY -= widthIn, transformInY -= widthIn ) {
                        uint8_t * imageOutX = imageOutY;
                        const uint8_t * imageOutXEnd = imageOutX + width;
                        const uint8_t * imageInX = imageInY;
                        const uint8_t * transformInX = transformInY;

                        for ( ; imageOutX != imageOutXEnd; ++imageOutX, ++imageInX, ++transformInX ) {
                            if ( *transformInX == 0 ) {
                                *imageOutX = *imageInX;
                            }
                        }
                    }
                }
                else {
                    for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, transformOutY += widthOut, imageInY -= widthIn, transformInY -= widthIn ) {
                        memcpy( imageOutY, imageInY, static_cast<size_t>( width ) );
                        memcpy( transformOutY, transformInY, static_cast<size_t>( width ) );
                    }
                }
            }
        }
        else {
            // Flip horizontally and vertically.
            if ( in.singleLayer() ) {
                const uint8_t * imageInY = in.image() + offsetIn + static_cast<ptrdiff_t>( height - 1 ) * widthIn + widthIn - 1;
                for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, imageInY -= widthIn ) {
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;
                    const uint8_t * imageInX = imageInY;

                    for ( ; imageOutX != imageOutXEnd; ++imageOutX, --imageInX ) {
                        *imageOutX = *imageInX;
                    }

                    if ( transformOutY != nullptr ) {
                        memset( transformOutY, static_cast<uint8_t>( 0 ), width );
                        transformOutY += widthOut;
                    }
                }
            }
            else {
                const uint8_t * imageInY = in.image() + offsetIn + static_cast<ptrdiff_t>( height - 1 ) * widthIn + widthIn - 1;
                const uint8_t * transformInY = in.transform() + offsetIn + static_cast<ptrdiff_t>( height - 1 ) * widthIn + widthIn - 1;

                for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, imageInY -= widthIn, transformInY -= widthIn ) {
                    uint8_t * imageOutX = imageOutY;
                    const uint8_t * imageOutXEnd = imageOutX + width;
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * transformInX = transformInY;

                    if ( transformOutY == nullptr ) {
                        for ( ; imageOutX != imageOutXEnd; ++imageOutX, --imageInX, --transformInX ) {
                            if ( *transformInX == 0 ) {
                                *imageOutX = *imageInX;
                            }
                        }
                    }
                    else {
                        for ( uint8_t * transformOutX = transformOutY; imageOutX != imageOutXEnd; ++imageOutX, ++transformOutX, --imageInX, --transformInX ) {
                            *imageOutX = *imageInX;
                            *transformOutX = *transformInX;
                        }
                        transformOutY += widthOut;
                    }
                }
            }
        }
    }

    Rect GetActiveROI( const Image & image, const uint8_t minTransformValue )
    {
        if ( image.empty() || image.singleLayer() ) {
            return {};
        }

        const int32_t width = image.width();
        const int32_t height = image.height();

        Rect area( -1, -1, -1, -1 );

        const uint8_t * inY = image.transform();
        for ( int32_t y = 0; y < height; ++y, inY += width ) {
            const uint8_t * inX = inY;
            for ( int32_t x = 0; x < width; ++x, ++inX ) {
                if ( *inX == 0 || *inX >= minTransformValue ) {
                    area.y = y;
                    break;
                }
            }
            if ( area.y >= 0 ) {
                break;
            }
        }

        if ( area.y < 0 ) {
            return {};
        }

        inY = image.transform() + width * ( height - 1 );
        for ( int32_t y = height - 1; y > area.y; --y, inY -= width ) {
            const uint8_t * inX = inY;
            for ( int32_t x = 0; x < width; ++x, ++inX ) {
                if ( *inX == 0 || *inX >= minTransformValue ) {
                    area.height = y - area.y + 1;
                    break;
                }
            }
            if ( area.height >= 0 ) {
                break;
            }
        }

        const uint8_t * inX = image.transform() + width * area.y;
        for ( int32_t x = 0; x < width; ++x, ++inX ) {
            inY = inX;
            for ( int32_t y = 0; y < area.height; ++y, inY += width ) {
                if ( *inY == 0 || *inY >= minTransformValue ) {
                    area.x = x;
                    break;
                }
            }
            if ( area.x >= 0 ) {
                break;
            }
        }

        inX = image.transform() + width * area.y + width - 1;
        for ( int32_t x = width - 1; x >= area.x; --x, --inX ) {
            inY = inX;
            for ( int32_t y = 0; y < area.height; ++y, inY += width ) {
                if ( *inY == 0 || *inY >= minTransformValue ) {
                    area.width = x - area.x + 1;
                    break;
                }
            }
            if ( area.width >= 0 ) {
                break;
            }
        }

        return area;
    }

    uint8_t GetColorId( const uint8_t red, const uint8_t green, const uint8_t blue )
    {
        return GetPALColorId( red / 4, green / 4, blue / 4 );
    }

    Sprite makeShadow( const Sprite & in, const Point & shadowOffset, const uint8_t transformId )
    {
        if ( in.empty() || shadowOffset.x > 0 || shadowOffset.y < 0 ) {
            return {};
        }

        const int32_t width = in.width();
        const int32_t height = in.height();

        Sprite out( width - shadowOffset.x, height + shadowOffset.y, in.x() + shadowOffset.x, in.y() );
        out.reset();

        assert( !out.empty() );

        if ( in.singleLayer() ) {
            FillTransform( out, 0, shadowOffset.y, width, height, transformId );
            return out;
        }

        const int32_t widthOut = out.width();

        const uint8_t * transformInY = in.transform();
        const uint8_t * transformInYEnd = transformInY + width * height;
        uint8_t * transformOutY = out.transform() + shadowOffset.y * widthOut;

        for ( ; transformInY != transformInYEnd; transformInY += width, transformOutY += widthOut ) {
            const uint8_t * transformInX = transformInY;
            uint8_t * transformOutX = transformOutY;
            const uint8_t * transformInXEnd = transformInX + width;

            for ( ; transformInX != transformInXEnd; ++transformInX, ++transformOutX ) {
                if ( *transformInX == 0 ) {
                    *transformOutX = transformId;
                }
            }
        }

        return out;
    }

    void ReplaceColorId( Image & image, const uint8_t oldColorId, const uint8_t newColorId )
    {
        if ( image.empty() ) {
            return;
        }

        // RGBA target: replace pixels matching palette[oldColorId] with palette[newColorId].
        // Phase 3 Display fast path: scan the indexed buffer (game-res, 1 byte per pixel)
        // and remap directly. The shader resolves to palette[newIdx] at sample time.
        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            uint8_t * idxBase = image.indexedBuffer();
            const uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxW = image.indexedStride();
                const int32_t idxH = image.indexedHeight();
                for ( int32_t row = 0; row < idxH; ++row ) {
                    uint8_t * idxRow = idxBase + static_cast<ptrdiff_t>( row ) * idxW;
                    const uint8_t * maskRow = maskBase + static_cast<ptrdiff_t>( row ) * idxW;
                    for ( int32_t col = 0; col < idxW; ++col ) {
                        // Only remap cells the shader resolves through the indexed channel.
                        if ( maskRow[col] != 0 && idxRow[col] == oldColorId ) {
                            idxRow[col] = newColorId;
                        }
                    }
                }
                image.markIndexedDirty( { 0, 0, idxW, idxH } );
                // Fall through to also rewrite RGBA: areas that have legitimate RGBA content
                // (hi-res monsters, video frames) might still match the old colour.
            }

            uint8_t oldR;
            uint8_t oldG;
            uint8_t oldB;
            paletteIdxToRGBA( oldColorId, oldR, oldG, oldB );
            uint8_t newR;
            uint8_t newG;
            uint8_t newB;
            paletteIdxToRGBA( newColorId, newR, newG, newB );

            const size_t total = static_cast<size_t>( image.bufferStride() ) * image.bufferHeight();
            uint8_t * px = image.image();
            for ( size_t i = 0; i < total; ++i, px += 4 ) {
                if ( px[0] == oldR && px[1] == oldG && px[2] == oldB && px[3] != 0 ) {
                    px[0] = newR;
                    px[1] = newG;
                    px[2] = newB;
                }
            }
            return;
        }

        uint8_t * data = image.image();
        const uint8_t * dataEnd = data + image.width() * image.height();

        for ( ; data != dataEnd; ++data ) {
            if ( *data == oldColorId ) {
                *data = newColorId;
            }
        }
    }

    void ReplaceColorIdByTransformId( Image & image, const uint8_t colorId, const uint8_t transformId )
    {
        if ( transformId > 15 || image.singleLayer() ) {
            return;
        }

        const int32_t width = image.width();
        const int32_t height = image.height();

        const uint8_t * imageIn = image.image();
        uint8_t * transformIn = image.transform();
        const uint8_t * imageInEnd = imageIn + height * width;
        for ( ; imageIn != imageInEnd; ++imageIn, ++transformIn ) {
            if ( *transformIn == 0 && *imageIn == colorId ) {
                *transformIn = transformId;
            }
        }
    }

    void ReplaceTransformIdByColorId( Image & image, const uint8_t transformId, const uint8_t colorId )
    {
        if ( image.empty() || image.singleLayer() ) {
            return;
        }

        const int32_t size = image.width() * image.height();

        uint8_t * imageIn = image.image();
        uint8_t * transformIn = image.transform();
        const uint8_t * imageInEnd = imageIn + size;
        for ( ; imageIn != imageInEnd; ++imageIn, ++transformIn ) {
            if ( *transformIn == transformId ) {
                *transformIn = 0U;
                *imageIn = colorId;
            }
        }
    }

    void Resize( const Image & in, Image & out )
    {
        if ( in.empty() || out.empty() ) {
            return;
        }

        Resize( in, 0, 0, in.width(), in.height(), out, 0, 0, out.width(), out.height() );
    }

    void Resize( const Image & in, const int32_t inX, const int32_t inY, const int32_t widthRoiIn, const int32_t heightRoiIn, Image & out, const int32_t outX,
                 const int32_t outY, const int32_t widthRoiOut, const int32_t heightRoiOut )
    {
        if ( !Validate( in, inX, inY, widthRoiIn, heightRoiIn ) || !Validate( out, outX, outY, widthRoiOut, heightRoiOut ) ) {
            return;
        }

        if ( widthRoiIn == widthRoiOut && heightRoiIn == heightRoiOut ) {
            Copy( in, inX, inY, out, outX, outY, widthRoiIn, heightRoiIn );
            return;
        }

        // RGBA out: nearest-neighbor scale, with destination block expansion on Display.
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            const int32_t widthIn = in.width();
            const bool inIsRGBA = ( in.format() == ImageFormat::RGBA_32BIT );
            const int32_t inStride = in.bufferStride();
            const float inScale = in.physicalScale();
            const float scale = out.physicalScale();
            const int32_t bufStride = out.bufferStride();
            const int32_t bufHeight = out.bufferHeight();
            uint8_t * outBase = out.image();

            std::vector<int32_t> positionX( widthRoiOut );
            for ( int32_t x = 0; x < widthRoiOut; ++x ) {
                positionX[x] = ( x * widthRoiIn ) / widthRoiOut;
            }

            for ( int32_t y = 0; y < heightRoiOut; ++y ) {
                const int32_t srcRowOffset = ( ( y * heightRoiIn ) / heightRoiOut );

                if ( inIsRGBA ) {
                    const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( inY + srcRowOffset ) * inScale );
                    const uint8_t * srcRow = in.image() + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;
                    for ( int32_t x = 0; x < widthRoiOut; ++x ) {
                        const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( inX + positionX[x] ) * inScale );
                        const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                        const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                        fillRGBABlock( outBase, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcPx[3] );
                    }
                }
                else {
                    const uint8_t * srcRow = in.image() + ( static_cast<ptrdiff_t>( inY + srcRowOffset ) * widthIn ) + inX;
                    const uint8_t * trRow = in.singleLayer() ? nullptr : ( in.transform() + ( static_cast<ptrdiff_t>( inY + srcRowOffset ) * widthIn ) + inX );
                    for ( int32_t x = 0; x < widthRoiOut; ++x ) {
                        const uint8_t srcIdx = srcRow[positionX[x]];
                        const uint8_t srcTr = trRow ? trRow[positionX[x]] : 0;
                        if ( srcTr == 1 ) {
                            continue;
                        }
                        const PhysicalBlock pb = toPhysicalBlock( outX + x, outY + y, scale, bufStride, bufHeight );
                        if ( srcTr > 1 && srcTr <= 5 ) {
                            shadeRGBABlock( outBase, pb, bufStride, shadowFactor[srcTr] );
                            continue;
                        }
                        uint8_t r;
                        uint8_t g;
                        uint8_t b;
                        paletteIdxToRGBA( srcIdx, r, g, b );
                        fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                    }
                }
            }
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();
        const int32_t offsetInY = inY * widthIn + inX;
        const int32_t offsetOutY = outY * widthOut + outX;

        const uint8_t * imageInY = in.image() + offsetInY;
        uint8_t * imageOutY = out.image() + offsetOutY;

        const uint8_t * imageOutYEnd = imageOutY + static_cast<ptrdiff_t>( widthOut ) * heightRoiOut;
        int32_t idY = 0;

        std::vector<int32_t> positionX( widthRoiOut );
        for ( int32_t x = 0; x < widthRoiOut; ++x ) {
            positionX[x] = ( x * widthRoiIn ) / widthRoiOut;
        }

        if ( in.singleLayer() ) {
            if ( !out.singleLayer() ) {
                uint8_t * transformY = out.transform() + static_cast<ptrdiff_t>( outY ) * widthOut + outX;
                const uint8_t * transformYEnd = transformY + static_cast<ptrdiff_t>( heightRoiOut ) * widthOut;

                for ( ; transformY != transformYEnd; transformY += widthOut ) {
                    memset( transformY, static_cast<uint8_t>( 0 ), widthRoiOut );
                }
            }

            for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, ++idY ) {
                uint8_t * imageOutX = imageOutY;
                const int32_t offset = ( ( idY * heightRoiIn ) / heightRoiOut ) * widthIn;
                const uint8_t * imageInX = imageInY + offset;

                for ( const int32_t posX : positionX ) {
                    *imageOutX = *( imageInX + posX );
                    ++imageOutX;
                }
            }
        }
        else if ( out.singleLayer() ) {
            const uint8_t * transformInY = in.transform() + offsetInY;

            for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, ++idY ) {
                uint8_t * imageOutX = imageOutY;
                const int32_t offset = ( ( idY * heightRoiIn ) / heightRoiOut ) * widthIn;
                const uint8_t * imageInX = imageInY + offset;
                const uint8_t * transformInX = transformInY + offset;

                for ( const int32_t posX : positionX ) {
                    const uint8_t * transformIn = transformInX + posX;
                    if ( *transformIn > 0 ) {
                        if ( *transformIn != 1 ) {
                            *imageOutX = *( transformTable + static_cast<ptrdiff_t>( *transformIn ) * 256 + *imageOutX );
                        }
                    }
                    else {
                        *imageOutX = *( imageInX + posX );
                    }
                    ++imageOutX;
                }
            }
        }
        else {
            const uint8_t * transformInY = in.transform() + offsetInY;
            uint8_t * transformOutY = out.transform() + offsetOutY;

            for ( ; imageOutY != imageOutYEnd; imageOutY += widthOut, transformOutY += widthOut, ++idY ) {
                uint8_t * imageOutX = imageOutY;
                uint8_t * transformOutX = transformOutY;
                const int32_t offset = ( ( idY * heightRoiIn ) / heightRoiOut ) * widthIn;
                const uint8_t * imageInX = imageInY + offset;
                const uint8_t * transformInX = transformInY + offset;

                for ( const int32_t posX : positionX ) {
                    *imageOutX = *( imageInX + posX );
                    *transformOutX = *( transformInX + posX );
                    ++imageOutX;
                    ++transformOutX;
                }
            }
        }
    }

    void SetPixel( Image & image, const int32_t x, const int32_t y, const uint8_t value )
    {
        if ( image.empty() || x >= image.width() || y >= image.height() || x < 0 || y < 0 ) {
            return;
        }

        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            uint8_t * idxBase = image.indexedBuffer();
            uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const ptrdiff_t off = static_cast<ptrdiff_t>( y ) * image.indexedStride() + x;
                idxBase[off] = value;
                maskBase[off] = 255;
                image.markIndexedDirty( { x, y, 1, 1 } );
                return;
            }

            uint8_t r;
            uint8_t g;
            uint8_t b;
            paletteIdxToRGBA( value, r, g, b );
            const PhysicalBlock pb = toPhysicalBlock( x, y, image.physicalScale(), image.bufferStride(), image.bufferHeight() );
            fillRGBABlock( image.image(), pb, image.bufferStride(), r, g, b, 255 );
            return;
        }

        const int32_t offset = y * image.width() + x;
        *( image.image() + offset ) = value;
        if ( !image.singleLayer() ) {
            *( image.transform() + offset ) = 0;
        }
    }

    void SetPixel( Image & image, const std::vector<Point> & points, const uint8_t value )
    {
        if ( image.empty() ) {
            return;
        }

        const int32_t width = image.width();
        const int32_t height = image.height();

        if ( image.format() == ImageFormat::RGBA_32BIT ) {
            uint8_t * idxBase = image.indexedBuffer();
            uint8_t * maskBase = image.maskBuffer();
            if ( idxBase != nullptr && maskBase != nullptr ) {
                const int32_t idxStride = image.indexedStride();
                int32_t minX = width;
                int32_t maxX = -1;
                int32_t minY = height;
                int32_t maxY = -1;
                for ( const Point & p : points ) {
                    if ( p.x < 0 || p.y < 0 || p.x >= width || p.y >= height ) {
                        continue;
                    }
                    const ptrdiff_t off = static_cast<ptrdiff_t>( p.y ) * idxStride + p.x;
                    idxBase[off] = value;
                    maskBase[off] = 255;
                    if ( p.x < minX ) {
                        minX = p.x;
                    }
                    if ( p.x > maxX ) {
                        maxX = p.x;
                    }
                    if ( p.y < minY ) {
                        minY = p.y;
                    }
                    if ( p.y > maxY ) {
                        maxY = p.y;
                    }
                }
                if ( maxX >= 0 ) {
                    image.markIndexedDirty( { minX, minY, maxX - minX + 1, maxY - minY + 1 } );
                }
                return;
            }

            uint8_t r;
            uint8_t g;
            uint8_t b;
            paletteIdxToRGBA( value, r, g, b );
            const float scale = image.physicalScale();
            const int32_t bufStride = image.bufferStride();
            const int32_t bufHeight = image.bufferHeight();
            uint8_t * outBase = image.image();
            for ( const Point & p : points ) {
                if ( p.x < 0 || p.y < 0 || p.x >= width || p.y >= height ) {
                    continue;
                }
                const PhysicalBlock pb = toPhysicalBlock( p.x, p.y, scale, bufStride, bufHeight );
                fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
            }
            // Conservative bbox clear over the whole image bounds is too coarse; skip for the
            // value == 0 case, which is rare and falls through to RGBA only.
            return;
        }

        const bool isDoubleLayer = !image.singleLayer();

        for ( const Point & point : points ) {
            if ( point.x >= width || point.y >= height || point.x < 0 || point.y < 0 ) {
                continue;
            }
            const int32_t offset = point.y * width + point.x;
            *( image.image() + offset ) = value;
            if ( isDoubleLayer ) {
                *( image.transform() + offset ) = 0;
            }
        }
    }

    void SetTransformPixel( Image & image, const int32_t x, const int32_t y, const uint8_t value )
    {
        if ( image.empty() || image.singleLayer() || x >= image.width() || y >= image.height() || x < 0 || y < 0 ) {
            return;
        }

        const int32_t offset = y * image.width() + x;
        *( image.image() + offset ) = 0;
        *( image.transform() + offset ) = value;
    }

    Image Stretch( const Image & in, int32_t inX, int32_t inY, int32_t widthIn, int32_t heightIn, const int32_t widthOut, const int32_t heightOut )
    {
        if ( !Validate( in, inX, inY, widthIn, heightIn ) || widthOut <= 0 || heightOut <= 0 ) {
            return {};
        }

        Image out;
        if ( in.singleLayer() ) {
            out._disableTransformLayer();
        }
        out.resize( widthOut, heightOut );

        const int32_t minWidth = widthIn < widthOut ? widthIn : widthOut;
        const int32_t minHeight = heightIn < heightOut ? heightIn : heightOut;

        const int32_t cornerWidth = minWidth / 3;
        const int32_t cornerHeight = minHeight / 3;
        const int32_t cornerX = inX + ( widthIn - cornerWidth ) / 2;
        const int32_t cornerY = inY + ( heightIn - cornerHeight ) / 2;
        const int32_t bodyWidth = minWidth - 2 * cornerWidth;
        const int32_t bodyHeight = minHeight - 2 * cornerHeight;

        const int32_t outX = ( widthOut - ( widthOut / bodyWidth ) * bodyWidth ) / 2;
        const int32_t outY = ( heightOut - ( heightOut / bodyHeight ) * bodyHeight ) / 2;

        if ( bodyWidth < widthOut && bodyHeight < heightOut ) {
            for ( int32_t y = 0; y < ( heightOut / bodyHeight ); ++y ) {
                for ( int32_t x = 0; x < ( widthOut / bodyWidth ); ++x ) {
                    Copy( in, cornerX, cornerY, out, outX + x * bodyWidth, outY + y * bodyHeight, bodyWidth, bodyHeight );
                }
            }
        }

        for ( int32_t x = 0; x < ( widthOut / bodyWidth ); ++x ) {
            const int32_t offsetX = outX + x * bodyWidth;
            Copy( in, cornerX, inY, out, offsetX, 0, bodyWidth, cornerHeight );
            Copy( in, cornerX, inY + heightIn - cornerHeight, out, offsetX, heightOut - cornerHeight, bodyWidth, cornerHeight );
        }

        for ( int32_t y = 0; y < ( heightOut / bodyHeight ); ++y ) {
            const int32_t offsetY = outY + y * bodyHeight;
            Copy( in, inX, cornerY, out, 0, offsetY, cornerWidth, bodyHeight );
            Copy( in, inX + widthIn - cornerWidth, cornerY, out, widthOut - cornerWidth, offsetY, cornerWidth, bodyHeight );
        }

        Copy( in, inX, inY, out, 0, 0, cornerWidth, cornerHeight );
        Copy( in, inX + widthIn - cornerWidth, inY, out, widthOut - cornerWidth, 0, cornerWidth, cornerHeight );
        Copy( in, inX, inY + heightIn - cornerHeight, out, 0, heightOut - cornerHeight, cornerWidth, cornerHeight );
        Copy( in, inX + widthIn - cornerWidth, inY + heightIn - cornerHeight, out, widthOut - cornerWidth, heightOut - cornerHeight, cornerWidth, cornerHeight );

        return out;
    }

    void SubpixelResize( const Image & in, Image & out )
    {
        if ( in.empty() || out.empty() ) {
            return;
        }

        SubpixelResize( in, 0, 0, in.width(), in.height(), out, 0, 0, out.width(), out.height() );
    }

    void SubpixelResize( const Image & in, const int32_t inX, const int32_t inY, const int32_t widthRoiIn, const int32_t heightRoiIn, Image & out, const int32_t outX,
                         const int32_t outY, const int32_t widthRoiOut, const int32_t heightRoiOut )
    {
        if ( !Validate( in, inX, inY, widthRoiIn, heightRoiIn ) || !Validate( out, outX, outY, widthRoiOut, heightRoiOut ) ) {
            return;
        }

        if ( widthRoiIn == widthRoiOut && heightRoiIn == heightRoiOut ) {
            Copy( in, inX, inY, out, outX, outY, widthRoiIn, heightRoiIn );
            return;
        }

        // RGBA out: degrade to nearest-neighbor Resize. SubpixelResize is rarely on the Display path.
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            Resize( in, inX, inY, widthRoiIn, heightRoiIn, out, outX, outY, widthRoiOut, heightRoiOut );
            return;
        }

        const int32_t widthIn = in.width();
        const int32_t widthOut = out.width();
        const int32_t offsetInY = inY * widthIn + inX;
        const int32_t offsetOutY = outY * widthOut + outX;

        const uint8_t * imageInY = in.image() + offsetInY;
        uint8_t * imageOutY = out.image() + offsetOutY;

        std::vector<double> positionX( widthRoiOut );
        for ( int32_t x = 0; x < widthRoiOut; ++x ) {
            positionX[x] = static_cast<double>( x * widthRoiIn ) / widthRoiOut;
        }

        const uint8_t * gamePalette = getGamePalette();

        if ( in.singleLayer() ) {
            if ( !out.singleLayer() ) {
                uint8_t * transformY = out.transform() + static_cast<ptrdiff_t>( outY ) * widthOut + outX;
                const uint8_t * transformYEnd = transformY + static_cast<ptrdiff_t>( heightRoiOut ) * widthOut;

                for ( ; transformY != transformYEnd; transformY += widthOut ) {
                    memset( transformY, static_cast<uint8_t>( 0 ), widthRoiOut );
                }
            }

            for ( int32_t y = 0; y < heightRoiOut; ++y, imageOutY += widthOut ) {
                const double posY = static_cast<double>( y * heightRoiIn ) / heightRoiOut;
                const int32_t startY = static_cast<int32_t>( posY ) * widthIn;
                const double coeffY = posY - static_cast<int32_t>( posY );

                uint8_t * imageOutX = imageOutY;

                for ( int32_t x = 0; x < widthRoiOut; ++x, ++imageOutX ) {
                    const double posX = positionX[x];
                    const int32_t startX = static_cast<int32_t>( posX );
                    const int32_t offsetIn = startY + startX;

                    const uint8_t * imageInX = imageInY + offsetIn;

                    if ( posX < widthRoiIn - 1 && posY < heightRoiIn - 1 ) {
                        const double coeffX = posX - startX;
                        const double coeff1 = ( 1 - coeffX ) * ( 1 - coeffY );
                        const double coeff2 = coeffX * ( 1 - coeffY );
                        const double coeff3 = ( 1 - coeffX ) * coeffY;
                        const double coeff4 = coeffX * coeffY;

                        const uint8_t * id1 = gamePalette + static_cast<size_t>( *imageInX ) * 3;
                        const uint8_t * id2 = gamePalette + static_cast<size_t>( *( imageInX + 1 ) ) * 3;
                        const uint8_t * id3 = gamePalette + static_cast<size_t>( *( imageInX + widthIn ) ) * 3;
                        const uint8_t * id4 = gamePalette + static_cast<size_t>( *( imageInX + widthIn + 1 ) ) * 3;

                        const double red = *id1 * coeff1 + *id2 * coeff2 + *id3 * coeff3 + *id4 * coeff4 + 0.5;
                        const double green = *( id1 + 1 ) * coeff1 + *( id2 + 1 ) * coeff2 + *( id3 + 1 ) * coeff3 + *( id4 + 1 ) * coeff4 + 0.5;
                        const double blue = *( id1 + 2 ) * coeff1 + *( id2 + 2 ) * coeff2 + *( id3 + 2 ) * coeff3 + *( id4 + 2 ) * coeff4 + 0.5;

                        *imageOutX = GetPALColorId( static_cast<uint8_t>( red ), static_cast<uint8_t>( green ), static_cast<uint8_t>( blue ) );
                    }
                    else {
                        *imageOutX = *imageInX;
                    }
                }
            }
        }
        else {
            const uint8_t * transformInY = in.transform() + offsetInY;
            const bool isOutNotSingleLayer = !out.singleLayer();
            uint8_t * transformOutY = isOutNotSingleLayer ? ( out.transform() + offsetOutY ) : nullptr;

            for ( int32_t y = 0; y < heightRoiOut; ++y, imageOutY += widthOut ) {
                const double posY = static_cast<double>( y * heightRoiIn ) / heightRoiOut;
                const int32_t startY = static_cast<int32_t>( posY ) * widthIn;
                const double coeffY = posY - static_cast<int32_t>( posY );

                uint8_t * imageOutX = imageOutY;
                uint8_t * transformOutX = transformOutY;

                for ( int32_t x = 0; x < widthRoiOut; ++x, ++imageOutX ) {
                    const double posX = positionX[x];
                    const int32_t startX = static_cast<int32_t>( posX );
                    const int32_t offsetIn = startY + startX;

                    const uint8_t * imageInX = imageInY + offsetIn;
                    const uint8_t * transformInX = transformInY + offsetIn;

                    if ( posX < widthRoiIn - 1 && posY < heightRoiIn - 1 && *transformInX == 0
                         && ( *( transformInX + 1 ) == 0 || *( transformInX + widthRoiIn ) == 0 ) ) {
                        if ( *( transformInX + 1 ) == 0 && *( transformInX + widthRoiIn ) == 0 && *( transformInX + widthRoiIn + 1 ) == 0 ) {
                            const double coeffX = posX - startX;
                            const double coeff1 = ( 1 - coeffX ) * ( 1 - coeffY );
                            const double coeff2 = coeffX * ( 1 - coeffY );
                            const double coeff3 = ( 1 - coeffX ) * coeffY;
                            const double coeff4 = coeffX * coeffY;

                            const uint8_t * id1 = gamePalette + static_cast<size_t>( *imageInX ) * 3;
                            const uint8_t * id2 = gamePalette + static_cast<size_t>( *( imageInX + 1 ) ) * 3;
                            const uint8_t * id3 = gamePalette + static_cast<size_t>( *( imageInX + widthIn ) ) * 3;
                            const uint8_t * id4 = gamePalette + static_cast<size_t>( *( imageInX + widthIn + 1 ) ) * 3;

                            const double red = *id1 * coeff1 + *id2 * coeff2 + *id3 * coeff3 + *id4 * coeff4 + 0.5;
                            const double green = *( id1 + 1 ) * coeff1 + *( id2 + 1 ) * coeff2 + *( id3 + 1 ) * coeff3 + *( id4 + 1 ) * coeff4 + 0.5;
                            const double blue = *( id1 + 2 ) * coeff1 + *( id2 + 2 ) * coeff2 + *( id3 + 2 ) * coeff3 + *( id4 + 2 ) * coeff4 + 0.5;

                            *imageOutX = GetPALColorId( static_cast<uint8_t>( red ), static_cast<uint8_t>( green ), static_cast<uint8_t>( blue ) );
                        }
                        else if ( *( transformInX + 1 ) != 0 && *( transformInX + widthRoiIn ) == 0 ) {
                            const double coeff1 = 1 - coeffY;

                            const uint8_t * id1 = gamePalette + static_cast<size_t>( *imageInX ) * 3;
                            const uint8_t * id3 = gamePalette + static_cast<size_t>( *( imageInX + widthIn ) ) * 3;

                            const double red = *id1 * coeff1 + *id3 * coeffY + 0.5;
                            const double green = *( id1 + 1 ) * coeff1 + *( id3 + 1 ) * coeffY + 0.5;
                            const double blue = *( id1 + 2 ) * coeff1 + *( id3 + 2 ) * coeffY + 0.5;

                            *imageOutX = GetPALColorId( static_cast<uint8_t>( red ), static_cast<uint8_t>( green ), static_cast<uint8_t>( blue ) );
                        }
                        else if ( *( transformInX + 1 ) == 0 && *( transformInX + widthRoiIn ) != 0 ) {
                            const double coeff2 = posX - startX;
                            const double coeff1 = 1 - coeff2;

                            const uint8_t * id1 = gamePalette + static_cast<size_t>( *imageInX ) * 3;
                            const uint8_t * id2 = gamePalette + static_cast<size_t>( *( imageInX + 1 ) ) * 3;

                            const double red = *id1 * coeff1 + *id2 * coeff2 + 0.5;
                            const double green = *( id1 + 1 ) * coeff1 + *( id2 + 1 ) * coeff2 + 0.5;
                            const double blue = *( id1 + 2 ) * coeff1 + *( id2 + 2 ) * coeff2 + 0.5;

                            *imageOutX = GetPALColorId( static_cast<uint8_t>( red ), static_cast<uint8_t>( green ), static_cast<uint8_t>( blue ) );
                        }
                        else if ( *( transformInX + 1 ) == 0 && *( transformInX + widthRoiIn ) == 0 && *( transformInX + widthRoiIn + 1 ) != 0 ) {
                            const double coeffX = posX - startX;
                            double coeff1 = ( 1 - coeffX ) * ( 1 - coeffY );
                            double coeff2 = coeffX * ( 1 - coeffY );
                            double coeff3 = ( 1 - coeffX ) * coeffY;
                            const double coeffSumm = coeff1 + coeff2 + coeff3;
                            coeff1 /= coeffSumm;
                            coeff2 /= coeffSumm;
                            coeff3 /= coeffSumm;

                            const uint8_t * id1 = gamePalette + static_cast<size_t>( *imageInX ) * 3;
                            const uint8_t * id2 = gamePalette + static_cast<size_t>( *( imageInX + 1 ) ) * 3;
                            const uint8_t * id3 = gamePalette + static_cast<size_t>( *( imageInX + widthIn ) ) * 3;

                            const double red = *id1 * coeff1 + *id2 * coeff2 + *id3 * coeff3 + 0.5;
                            const double green = *( id1 + 1 ) * coeff1 + *( id2 + 1 ) * coeff2 + *( id3 + 1 ) * coeff3 + 0.5;
                            const double blue = *( id1 + 2 ) * coeff1 + *( id2 + 2 ) * coeff2 + *( id3 + 2 ) * coeff3 + 0.5;

                            *imageOutX = GetPALColorId( static_cast<uint8_t>( red ), static_cast<uint8_t>( green ), static_cast<uint8_t>( blue ) );
                        }
                    }
                    else {
                        if ( isOutNotSingleLayer || *transformInX == 0 ) {
                            *imageOutX = *imageInX;
                        }
                        else if ( *transformInX != 1 ) {
                            *imageOutX = *( transformTable + static_cast<ptrdiff_t>( *transformInX ) * 256 + *imageOutX );
                        }
                    }

                    if ( isOutNotSingleLayer ) {
                        *transformOutX = *transformInX;
                        ++transformOutX;
                    }
                }

                if ( isOutNotSingleLayer ) {
                    transformOutY += widthOut;
                }
            }
        }
    }

    void Transpose( const Image & in, Image & out )
    {
        assert( !out.empty() );

        if ( in.empty() || in.width() != out.height() || in.height() != out.width() ) {
            out.reset();
            return;
        }
        const int32_t width = in.width();
        const int32_t height = in.height();

        // RGBA path: per-pixel transpose.
        if ( out.format() == ImageFormat::RGBA_32BIT ) {
            assert( in.format() == ImageFormat::RGBA_32BIT );
            const int32_t outW = out.width();
            for ( int32_t y = 0; y < height; ++y ) {
                const uint8_t * srcRow = in.image() + ( static_cast<ptrdiff_t>( y ) * width ) * 4;
                for ( int32_t x = 0; x < width; ++x ) {
                    const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( x ) * 4;
                    uint8_t * dstPx = out.image() + ( static_cast<ptrdiff_t>( x ) * outW + y ) * 4;
                    dstPx[0] = srcPx[0];
                    dstPx[1] = srcPx[1];
                    dstPx[2] = srcPx[2];
                    dstPx[3] = srcPx[3];
                }
            }
            return;
        }

        const uint8_t * imageInY = in.image();
        const uint8_t * imageInYEnd = imageInY + width * height;
        uint8_t * imageOutX = out.image();

        if ( in.singleLayer() ) {
            if ( out.singleLayer() ) {
                for ( ; imageInY != imageInYEnd; imageInY += width, ++imageOutX ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * imageInXEnd = imageInX + width;
                    uint8_t * imageOutY = imageOutX;
                    for ( ; imageInX != imageInXEnd; ++imageInX, imageOutY += height ) {
                        *imageOutY = *imageInX;
                    }
                }
            }
            else {
                uint8_t * transformOutX = out.transform();
                for ( ; imageInY != imageInYEnd; imageInY += width, ++imageOutX, ++transformOutX ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * imageInXEnd = imageInX + width;
                    uint8_t * imageOutY = imageOutX;
                    uint8_t * transformOutY = transformOutX;
                    for ( ; imageInX != imageInXEnd; ++imageInX, imageOutY += height, transformOutY += height ) {
                        *imageOutY = *imageInX;
                        *transformOutY = 0;
                    }
                }
            }
        }
        else {
            const uint8_t * transformInY = in.transform();
            if ( out.singleLayer() ) {
                for ( ; imageInY != imageInYEnd; imageInY += width, transformInY += width, ++imageOutX ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * imageInXEnd = imageInX + width;
                    uint8_t * imageOutY = imageOutX;
                    const uint8_t * transformInX = transformInY;
                    for ( ; imageInX != imageInXEnd; ++imageInX, ++transformInX, imageOutY += height ) {
                        if ( *transformInX == 0 ) {
                            *imageOutY = *imageInX;
                        }
                    }
                }
            }
            else {
                uint8_t * transformOutX = out.transform();
                for ( ; imageInY != imageInYEnd; imageInY += width, transformInY += width, ++imageOutX, ++transformOutX ) {
                    const uint8_t * imageInX = imageInY;
                    const uint8_t * imageInXEnd = imageInX + width;
                    uint8_t * imageOutY = imageOutX;
                    const uint8_t * transformInX = transformInY;
                    uint8_t * transformOutY = transformOutX;
                    for ( ; imageInX != imageInXEnd; ++imageInX, ++transformInX, imageOutY += height, transformOutY += height ) {
                        *imageOutY = *imageInX;
                        *transformOutY = *transformInX;
                    }
                }
            }
        }
    }

    void updateShadow( Image & image, const Point & shadowOffset, const uint8_t transformId, const bool connectCorners )
    {
        const int32_t imageWidth = image.width();
        const int32_t imageHeight = image.height();

        if ( image.empty() || image.singleLayer() || ( std::abs( shadowOffset.x ) >= imageWidth ) || ( std::abs( shadowOffset.y ) >= imageHeight )
             || shadowOffset == Point() ) {
            return;
        }

        const int32_t width = imageWidth - std::abs( shadowOffset.x );
        const int32_t height = imageHeight - std::abs( shadowOffset.y );

        const uint8_t * transformInY = image.transform();
        uint8_t * transformOutY = image.transform();

        int32_t cornerOffsetX;
        int32_t cornerOffsetY;

        if ( shadowOffset.x > 0 ) {
            transformOutY += shadowOffset.x;
            cornerOffsetX = 1;
        }
        else {
            transformInY -= shadowOffset.x;
            cornerOffsetX = -1;
        }

        if ( shadowOffset.y > 0 ) {
            transformOutY += imageWidth * shadowOffset.y;
            cornerOffsetY = imageWidth;
        }
        else {
            transformInY -= imageWidth * shadowOffset.y;
            cornerOffsetY = -imageWidth;
        }

        const uint8_t * transformOutYEnd = transformOutY + imageWidth * height;

        for ( ; transformOutY != transformOutYEnd; transformInY += imageWidth, transformOutY += imageWidth ) {
            const uint8_t * transformInX = transformInY;
            uint8_t * transformOutX = transformOutY;
            const uint8_t * transformOutXEnd = transformOutX + width;

            for ( ; transformOutX != transformOutXEnd; ++transformInX, ++transformOutX ) {
                if ( *transformOutX == 1
                     && ( *transformInX == 0 || ( connectCorners && *( transformInX + cornerOffsetX ) == 0 && *( transformInX + cornerOffsetY ) == 0 ) ) ) {
                    *transformOutX = transformId;
                }
            }
        }
    }

    // ===== RGBA-only public helpers =====

    // BlitRGBAScaled: hi-res RGBA source -> RGBA destination, scaled to game-coord rect
    // (dstW, dstH). When out is the physical-resolution Display, the destination physical
    // rect is (dstW * scale, dstH * scale), so we sample the source PNG straight into final
    // physical pixels — single nearest-neighbour downscale, no SDL bilinear pass on top.
    void BlitRGBAScaled( const Image & in, Image & out, const int32_t outX, const int32_t outY, const int32_t dstW, const int32_t dstH, const bool flip )
    {
        if ( in.empty() || out.empty() || dstW <= 0 || dstH <= 0 ) {
            return;
        }
        assert( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT );

        // Phase 3: BlitRGBAScaled iterates per physical destination pixel and writes only
        // where the (downsampled) source block has rgbCount > 0. For a game pixel whose
        // source block is partially transparent, only some of the scale² physical pixels
        // get a fresh RGBA write — the rest keep stale content. Marking the whole game
        // pixel as mask=0 (RGBA wins) then exposes those stale physical pixels (often
        // zeros from initial reset, sometimes leftovers from older frames), producing
        // edge artefacts on hi-res monster downscales.
        //
        // Materialize the dst ROI first: every mask=255 cell becomes mask=0+idx=0 with
        // palette[idx] written into its scale² RGBA block. After this, the source-
        // transparent physical pixels show the underlying slot frame and the source-
        // opaque physical pixels get the freshly averaged RGBA on top.
        materializeIndexedRoi( out, outX, outY, dstW, dstH );

        uint8_t * outMaskBase = out.maskBuffer();
        uint8_t * outIdxBase = out.indexedBuffer();
        const int32_t outIdxStride = ( outMaskBase != nullptr ) ? out.indexedStride() : 0;
        const int32_t outFbW = out.width();
        const int32_t outFbH = out.height();

        const int32_t srcW = in.width();
        const int32_t srcH = in.height();
        const int32_t outW = out.width();
        const int32_t outH = out.height();

        const float scale = out.physicalScale();
        const int32_t bufStride = out.bufferStride();
        const int32_t bufHeight = out.bufferHeight();

        // Game-coord clipping.
        const int32_t startX = std::max( outX, 0 );
        const int32_t startY = std::max( outY, 0 );
        const int32_t endX = std::min( outX + dstW, outW );
        const int32_t endY = std::min( outY + dstH, outH );
        if ( startX >= endX || startY >= endY ) {
            return;
        }

        // Convert game-coord destination span -> physical-pixel destination span.
        const int32_t pStartX = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( startX ) * scale ) );
        const int32_t pStartY = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( startY ) * scale ) );
        const int32_t pEndX = std::min<int32_t>( bufStride, static_cast<int32_t>( static_cast<float>( endX ) * scale ) );
        const int32_t pEndY = std::min<int32_t>( bufHeight, static_cast<int32_t>( static_cast<float>( endY ) * scale ) );
        if ( pStartX >= pEndX || pStartY >= pEndY ) {
            return;
        }

        // Physical-pixel extent of the dst rect (used for source mapping).
        const int32_t pDstW = static_cast<int32_t>( static_cast<float>( dstW ) * scale );
        const int32_t pDstH = static_cast<int32_t>( static_cast<float>( dstH ) * scale );
        const int32_t pOutX = static_cast<int32_t>( static_cast<float>( outX ) * scale );
        const int32_t pOutY = static_cast<int32_t>( static_cast<float>( outY ) * scale );
        if ( pDstW <= 0 || pDstH <= 0 ) {
            return;
        }

        const uint8_t * srcData = in.image();
        uint8_t * dstData = out.image();

        // For significant downscales (e.g. army-bar mini-icon: ~1136×736 source → ~96×96
        // physical), nearest-neighbour throws away ~99% of the source pixels and produces a
        // chunky, aliased result. Use alpha-weighted box-filter averaging across the source
        // block that each physical destination pixel covers — same approach as
        // writeIndexedSpriteFromRGBA. For 1:1 / minor scaling / upscaling each block reduces
        // to a single source pixel, which collapses to nearest-neighbour at no extra cost.
        for ( int32_t py = pStartY; py < pEndY; ++py ) {
            const int32_t relPy = py - pOutY;
            const int32_t sy0 = ( relPy * srcH ) / pDstH;
            const int32_t sy1 = std::max( sy0 + 1, ( ( relPy + 1 ) * srcH ) / pDstH );
            uint8_t * dstRow = dstData + static_cast<ptrdiff_t>( py ) * bufStride * 4;

            for ( int32_t px = pStartX; px < pEndX; ++px ) {
                const int32_t relPx = px - pOutX;
                const int32_t flippedX = flip ? ( pDstW - 1 - relPx ) : relPx;
                const int32_t sx0 = ( flippedX * srcW ) / pDstW;
                const int32_t sx1 = std::max( sx0 + 1, ( ( flippedX + 1 ) * srcW ) / pDstW );

                uint32_t rSum = 0;
                uint32_t gSum = 0;
                uint32_t bSum = 0;
                uint32_t aSum = 0;
                uint32_t rgbCount = 0;
                uint32_t totalCount = 0;
                for ( int32_t sy = sy0; sy < sy1; ++sy ) {
                    const uint8_t * srcRow = srcData + static_cast<ptrdiff_t>( sy ) * srcW * 4;
                    for ( int32_t sx = sx0; sx < sx1; ++sx ) {
                        const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( sx ) * 4;
                        const uint8_t alpha = srcPx[3];
                        aSum += alpha;
                        ++totalCount;
                        if ( alpha > 0 ) {
                            rSum += srcPx[0];
                            gSum += srcPx[1];
                            bSum += srcPx[2];
                            ++rgbCount;
                        }
                    }
                }

                if ( rgbCount == 0 ) {
                    // Whole source block fully transparent — leave destination as-is so
                    // sprite shapes still composite over the existing framebuffer (matches
                    // the alpha==0 skip in the previous nearest-neighbour code path).
                    continue;
                }

                uint8_t * dstPx = dstRow + static_cast<ptrdiff_t>( px ) * 4;
                dstPx[0] = static_cast<uint8_t>( rSum / rgbCount );
                dstPx[1] = static_cast<uint8_t>( gSum / rgbCount );
                dstPx[2] = static_cast<uint8_t>( bSum / rgbCount );
                dstPx[3] = static_cast<uint8_t>( aSum / totalCount );

                // Mark this game pixel as RGBA-resolved so the shader's sentinel picks
                // the just-written RGBA. Multiple physical pixels covering one game
                // pixel write the same byte redundantly — cheap and keeps the inner
                // loop branch-free.
                if ( outMaskBase != nullptr ) {
                    const int32_t gx = static_cast<int32_t>( static_cast<float>( px ) / scale );
                    const int32_t gy = static_cast<int32_t>( static_cast<float>( py ) / scale );
                    if ( gx >= 0 && gx < outFbW && gy >= 0 && gy < outFbH ) {
                        const ptrdiff_t off = static_cast<ptrdiff_t>( gy ) * outIdxStride + gx;
                        outMaskBase[off] = 0;
                        outIdxBase[off] = 0;
                    }
                }
            }
        }
        if ( outMaskBase != nullptr ) {
            out.markIndexedDirty( { startX, startY, endX - startX, endY - startY } );
        }
    }

    void BlitRGBAScaledAlpha( const Image & in, Image & out, const int32_t outX, const int32_t outY, const int32_t dstW, const int32_t dstH, const uint8_t alpha,
                              const bool flip )
    {
        if ( in.empty() || out.empty() || dstW <= 0 || dstH <= 0 || alpha == 0 ) {
            return;
        }
        if ( alpha == 255 ) {
            BlitRGBAScaled( in, out, outX, outY, dstW, dstH, flip );
            return;
        }
        assert( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT );

        // Phase 3: alpha blend reads the destination RGBA, so we materialize the underlying
        // indexed pixels into RGBA first; otherwise the blend baseline is the stale RGBA
        // leftovers (zero / previous frame), which produces wrong colours. materializeIndexedRoi
        // clears the mask in the touched pixels so the shader resolves through the freshly
        // blended RGBA going forward.
        materializeIndexedRoi( out, outX, outY, dstW, dstH );

        const int32_t srcW = in.width();
        const int32_t srcH = in.height();
        const int32_t outW = out.width();
        const int32_t outH = out.height();

        const float scale = out.physicalScale();
        const int32_t bufStride = out.bufferStride();
        const int32_t bufHeight = out.bufferHeight();

        const int32_t startX = std::max( outX, 0 );
        const int32_t startY = std::max( outY, 0 );
        const int32_t endX = std::min( outX + dstW, outW );
        const int32_t endY = std::min( outY + dstH, outH );
        if ( startX >= endX || startY >= endY ) {
            return;
        }

        const int32_t pStartX = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( startX ) * scale ) );
        const int32_t pStartY = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( startY ) * scale ) );
        const int32_t pEndX = std::min<int32_t>( bufStride, static_cast<int32_t>( static_cast<float>( endX ) * scale ) );
        const int32_t pEndY = std::min<int32_t>( bufHeight, static_cast<int32_t>( static_cast<float>( endY ) * scale ) );
        if ( pStartX >= pEndX || pStartY >= pEndY ) {
            return;
        }

        const int32_t pDstW = static_cast<int32_t>( static_cast<float>( dstW ) * scale );
        const int32_t pDstH = static_cast<int32_t>( static_cast<float>( dstH ) * scale );
        const int32_t pOutX = static_cast<int32_t>( static_cast<float>( outX ) * scale );
        const int32_t pOutY = static_cast<int32_t>( static_cast<float>( outY ) * scale );
        if ( pDstW <= 0 || pDstH <= 0 ) {
            return;
        }

        const uint8_t * srcData = in.image();
        uint8_t * dstData = out.image();

        // Same alpha-weighted box-filter strategy as BlitRGBAScaled — average the source
        // block that each physical destination pixel covers, then alpha-blend the averaged
        // colour over the existing destination. Degenerates to nearest-neighbour at 1:1
        // scale or upscaling.
        for ( int32_t py = pStartY; py < pEndY; ++py ) {
            const int32_t relPy = py - pOutY;
            const int32_t sy0 = ( relPy * srcH ) / pDstH;
            const int32_t sy1 = std::max( sy0 + 1, ( ( relPy + 1 ) * srcH ) / pDstH );
            uint8_t * dstRow = dstData + static_cast<ptrdiff_t>( py ) * bufStride * 4;

            for ( int32_t px = pStartX; px < pEndX; ++px ) {
                const int32_t relPx = px - pOutX;
                const int32_t flippedX = flip ? ( pDstW - 1 - relPx ) : relPx;
                const int32_t sx0 = ( flippedX * srcW ) / pDstW;
                const int32_t sx1 = std::max( sx0 + 1, ( ( flippedX + 1 ) * srcW ) / pDstW );

                uint32_t rSum = 0;
                uint32_t gSum = 0;
                uint32_t bSum = 0;
                uint32_t aSum = 0;
                uint32_t rgbCount = 0;
                uint32_t totalCount = 0;
                for ( int32_t sy = sy0; sy < sy1; ++sy ) {
                    const uint8_t * srcRow = srcData + static_cast<ptrdiff_t>( sy ) * srcW * 4;
                    for ( int32_t sx = sx0; sx < sx1; ++sx ) {
                        const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( sx ) * 4;
                        const uint8_t a = srcPx[3];
                        aSum += a;
                        ++totalCount;
                        if ( a > 0 ) {
                            rSum += srcPx[0];
                            gSum += srcPx[1];
                            bSum += srcPx[2];
                            ++rgbCount;
                        }
                    }
                }

                if ( rgbCount == 0 ) {
                    continue;
                }
                const uint32_t avgAlpha = aSum / totalCount;
                const uint32_t srcA = ( avgAlpha * alpha ) / 255;
                if ( srcA == 0 ) {
                    continue;
                }
                uint8_t * dstPx = dstRow + static_cast<ptrdiff_t>( px ) * 4;
                const uint32_t dstA = dstPx[3];
                const uint32_t invSrcA = 255 - srcA;
                const uint32_t srcR = rSum / rgbCount;
                const uint32_t srcG = gSum / rgbCount;
                const uint32_t srcB = bSum / rgbCount;
                dstPx[0] = static_cast<uint8_t>( ( srcR * srcA + dstPx[0] * invSrcA ) / 255 );
                dstPx[1] = static_cast<uint8_t>( ( srcG * srcA + dstPx[1] * invSrcA ) / 255 );
                dstPx[2] = static_cast<uint8_t>( ( srcB * srcA + dstPx[2] * invSrcA ) / 255 );
                dstPx[3] = static_cast<uint8_t>( std::min( srcA + ( dstA * invSrcA ) / 255, static_cast<uint32_t>( 255 ) ) );
            }
        }
    }

    void BlitRGBAAlpha( const Image & in, Image & out, const int32_t outX, const int32_t outY, const uint8_t alpha, const bool flip )
    {
        if ( in.empty() || out.empty() || alpha == 0 ) {
            return;
        }
        assert( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT );

        const int32_t srcW = in.width();
        const int32_t srcH = in.height();
        const int32_t dstW = out.width();
        const int32_t dstH = out.height();

        // Phase 3: src_over alpha blend reads the existing dst — materialize indexed
        // first so the blend baseline is correct. Clears the mask at touched pixels.
        materializeIndexedRoi( out, outX, outY, srcW, srcH );

        const int32_t startX = std::max( outX, 0 );
        const int32_t startY = std::max( outY, 0 );
        const int32_t endX = std::min( outX + srcW, dstW );
        const int32_t endY = std::min( outY + srcH, dstH );

        if ( startX >= endX || startY >= endY ) {
            return;
        }

        const int32_t inStride = in.bufferStride();
        const float inScale = in.physicalScale();
        const float scale = out.physicalScale();
        const int32_t bufStride = out.bufferStride();
        const int32_t bufHeight = out.bufferHeight();
        const uint8_t * srcData = in.image();
        uint8_t * dstData = out.image();

        for ( int32_t y = startY; y < endY; ++y ) {
            const int32_t srcGameY = y - outY;
            const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( srcGameY ) * inScale );
            const uint8_t * srcRow = srcData + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;

            for ( int32_t x = startX; x < endX; ++x ) {
                const int32_t srcGameX = flip ? ( srcW - 1 - ( x - outX ) ) : ( x - outX );
                const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcGameX ) * inScale );
                const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                if ( srcPx[3] == 0 ) {
                    continue;
                }
                const uint32_t srcA = ( static_cast<uint32_t>( srcPx[3] ) * alpha ) / 255;
                if ( srcA == 0 ) {
                    continue;
                }
                const PhysicalBlock pb = toPhysicalBlock( x, y, scale, bufStride, bufHeight );
                blendRGBABlock( dstData, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcA );
            }
        }
    }

    void DrawLineRGBA( Image & image, const Point & start, const Point & end, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a )
    {
        if ( image.empty() || a == 0 ) {
            return;
        }
        assert( image.format() == ImageFormat::RGBA_32BIT );

        const int32_t imgW = image.width();
        const int32_t imgH = image.height();

        // Phase 3: line draws into the RGBA buffer, but on Display the visible pixels for
        // mask=255 cells live in the indexed channel — the GPU shader resolves palette[idx]
        // and the line write would be invisible. Materialize the line's bounding box first
        // so every cell becomes mask=0 RGBA; the alpha-blend case (a < 255) also needs this
        // because blendRGBABlock reads the existing RGBA, which would be stale on indexed
        // cells. Materialize is a no-op on non-Display targets.
        const int32_t bboxMinX = std::min( start.x, end.x );
        const int32_t bboxMinY = std::min( start.y, end.y );
        const int32_t bboxMaxX = std::max( start.x, end.x );
        const int32_t bboxMaxY = std::max( start.y, end.y );
        materializeIndexedRoi( image, bboxMinX, bboxMinY, bboxMaxX - bboxMinX + 1, bboxMaxY - bboxMinY + 1 );

        const float scale = image.physicalScale();
        const int32_t bufStride = image.bufferStride();
        const int32_t bufHeight = image.bufferHeight();
        uint8_t * outBase = image.image();

        int32_t x0 = start.x;
        int32_t y0 = start.y;
        const int32_t x1 = end.x;
        const int32_t y1 = end.y;

        const int32_t dx = std::abs( x1 - x0 );
        const int32_t dy = -std::abs( y1 - y0 );
        const int32_t sx = ( x0 < x1 ) ? 1 : -1;
        const int32_t sy = ( y0 < y1 ) ? 1 : -1;
        int32_t err = dx + dy;

        while ( true ) {
            if ( x0 >= 0 && x0 < imgW && y0 >= 0 && y0 < imgH ) {
                const PhysicalBlock pb = toPhysicalBlock( x0, y0, scale, bufStride, bufHeight );
                if ( a == 255 ) {
                    fillRGBABlock( outBase, pb, bufStride, r, g, b, 255 );
                }
                else {
                    blendRGBABlock( outBase, pb, bufStride, r, g, b, a );
                }
            }

            if ( x0 == x1 && y0 == y1 ) {
                break;
            }

            const int32_t e2 = 2 * err;
            if ( e2 >= dy ) {
                err += dy;
                x0 += sx;
            }
            if ( e2 <= dx ) {
                err += dx;
                y0 += sy;
            }
        }
    }

    // CopyRGBA: rectangular RGBA region copy. Game coords on both sides; reads in at its
    // physical-pixel stride, writes out as a physical-pixel block per game pixel. Same-scale
    // (the common scratch -> scratch / Display -> Display case) collapses to a scaled memcpy.
    void CopyRGBA( const Image & in, const int32_t inX, const int32_t inY, Image & out, const int32_t outX, const int32_t outY, const int32_t w, const int32_t h )
    {
        if ( in.empty() || out.empty() || w <= 0 || h <= 0 ) {
            return;
        }
        assert( in.format() == ImageFormat::RGBA_32BIT && out.format() == ImageFormat::RGBA_32BIT );

        // Phase 3: when reading from Display, the visible pixel for mask=255 cells lives in
        // the indexed channel; the RGBA backing is stale. Materialize the source ROI first
        // so the read below picks up palette[idx] for those cells. Const-cast is sound:
        // materialize is visibly idempotent (the displayed content is unchanged, just
        // promoted from indexed-channel storage to RGBA-channel storage). No-op on
        // non-Display sources (no indexed/mask buffers). Without this, Battle::Interface::
        // _redrawActionDeathWaveSpell captures garbage where low-res monsters were and the
        // wave effect renders them as solid black holes.
        materializeIndexedRoi( const_cast<Image &>( in ), inX, inY, w, h );

        clearIndexedBboxOnDisplay( out, outX, outY, w, h );

        const int32_t srcW = in.width();
        const int32_t srcH = in.height();
        const int32_t dstW = out.width();
        const int32_t dstH = out.height();

        const int32_t srcStartX = std::max( inX, 0 );
        const int32_t srcStartY = std::max( inY, 0 );
        const int32_t srcEndX = std::min( inX + w, srcW );
        const int32_t srcEndY = std::min( inY + h, srcH );

        if ( srcStartX >= srcEndX || srcStartY >= srcEndY ) {
            return;
        }

        const int32_t copyW = std::min( srcEndX - srcStartX, dstW - std::max( outX, 0 ) );
        const int32_t copyH = std::min( srcEndY - srcStartY, dstH - std::max( outY, 0 ) );

        if ( copyW <= 0 || copyH <= 0 ) {
            return;
        }

        const int32_t dstStartX = std::max( outX + ( srcStartX - inX ), 0 );
        const int32_t dstStartY = std::max( outY + ( srcStartY - inY ), 0 );

        const int32_t inStride = in.bufferStride();
        const float inScale = in.physicalScale();
        const float scale = out.physicalScale();
        const int32_t bufStride = out.bufferStride();
        const int32_t bufHeight = out.bufferHeight();
        const uint8_t * srcData = in.image();
        uint8_t * dstData = out.image();

        // Fast path: matching scale on both sides — copy directly at physical-pixel resolution.
        // See note in BlitRGBAScaled — both values are derived integer ratios.
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
        if ( inScale == scale ) {
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif
            const int32_t pSrcX = static_cast<int32_t>( static_cast<float>( srcStartX ) * inScale );
            const int32_t pSrcY = static_cast<int32_t>( static_cast<float>( srcStartY ) * inScale );
            const int32_t pDstX = static_cast<int32_t>( static_cast<float>( dstStartX ) * scale );
            const int32_t pDstY = static_cast<int32_t>( static_cast<float>( dstStartY ) * scale );
            const int32_t pW = static_cast<int32_t>( static_cast<float>( copyW ) * scale );
            const int32_t pH = static_cast<int32_t>( static_cast<float>( copyH ) * scale );
            for ( int32_t row = 0; row < pH; ++row ) {
                const uint8_t * srcRow = srcData + ( static_cast<ptrdiff_t>( pSrcY + row ) * inStride + pSrcX ) * 4;
                uint8_t * dstRow = dstData + ( static_cast<ptrdiff_t>( pDstY + row ) * bufStride + pDstX ) * 4;
                memcpy( dstRow, srcRow, static_cast<size_t>( pW ) * 4 );
            }
            return;
        }

        // Slow path: scale mismatch — sample source, expand destination per game pixel.
        for ( int32_t row = 0; row < copyH; ++row ) {
            const int32_t srcPhysY = static_cast<int32_t>( static_cast<float>( srcStartY + row ) * inScale );
            const uint8_t * srcRow = srcData + static_cast<ptrdiff_t>( srcPhysY ) * inStride * 4;
            for ( int32_t col = 0; col < copyW; ++col ) {
                const int32_t srcPhysX = static_cast<int32_t>( static_cast<float>( srcStartX + col ) * inScale );
                const uint8_t * srcPx = srcRow + static_cast<ptrdiff_t>( srcPhysX ) * 4;
                const PhysicalBlock pb = toPhysicalBlock( dstStartX + col, dstStartY + row, scale, bufStride, bufHeight );
                fillRGBABlock( dstData, pb, bufStride, srcPx[0], srcPx[1], srcPx[2], srcPx[3] );
            }
        }
    }

    // DimRGBA: multiply RGB by factor (preserve alpha) over a game-coord rect. Iterates the
    // physical-pixel block backing each game pixel directly.
    void DimRGBA( Image & image, const int32_t x, const int32_t y, const int32_t width, const int32_t height, const float factor )
    {
        if ( image.empty() || factor >= 1.0f ) {
            return;
        }
        assert( image.format() == ImageFormat::RGBA_32BIT );

        const int32_t imgW = image.width();
        const int32_t imgH = image.height();

        const int32_t startX = std::max( x, 0 );
        const int32_t startY = std::max( y, 0 );
        const int32_t endX = std::min( x + width, imgW );
        const int32_t endY = std::min( y + height, imgH );

        if ( startX >= endX || startY >= endY ) {
            return;
        }

        // Phase 3: pull mask=255 indexed cells into RGBA before the dim — without this the
        // shader keeps showing palette[idx] over the dimmed RGBA, so the dim only takes
        // visible effect on the half of the battle area that's already mask=0 (the pre-
        // rendered _battleGroundRGBA). Used by Death Wave, Holy Shout, Earthquake, and the
        // bloodlust scratch path (the latter is non-Display so materialize is a no-op).
        // Shadow markers (mask=0, idx in [2..5]) deliberately survive: dim then shadow at
        // sample time is the right order.
        materializeIndexedRoi( image, startX, startY, endX - startX, endY - startY );

        const float f = std::max( factor, 0.0f );
        const float scale = image.physicalScale();
        const int32_t bufStride = image.bufferStride();
        const int32_t bufHeight = image.bufferHeight();
        uint8_t * data = image.image();

        // Compute the physical-pixel rect once and dim it directly (no per-game-pixel block math
        // for an op that's already per-pixel uniform).
        const int32_t pStartX = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( startX ) * scale ) );
        const int32_t pStartY = std::max<int32_t>( 0, static_cast<int32_t>( static_cast<float>( startY ) * scale ) );
        const int32_t pEndX = std::min<int32_t>( bufStride, static_cast<int32_t>( static_cast<float>( endX ) * scale ) );
        const int32_t pEndY = std::min<int32_t>( bufHeight, static_cast<int32_t>( static_cast<float>( endY ) * scale ) );
        for ( int32_t row = pStartY; row < pEndY; ++row ) {
            uint8_t * rowData = data + static_cast<ptrdiff_t>( row ) * bufStride * 4;
            for ( int32_t col = pStartX; col < pEndX; ++col ) {
                uint8_t * px = rowData + static_cast<ptrdiff_t>( col ) * 4;
                if ( px[3] > 0 ) {
                    px[0] = static_cast<uint8_t>( static_cast<float>( px[0] ) * f );
                    px[1] = static_cast<uint8_t>( static_cast<float>( px[1] ) * f );
                    px[2] = static_cast<uint8_t>( static_cast<float>( px[2] ) * f );
                }
            }
        }
    }
}
