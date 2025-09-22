#!/bin/bash

# Makes use of swavgen
# https://github.com/ymich9963/swavgen
# v1.1.5+

rm -f *.wav
rm -f *.flac

./swavgen sine -s 44100 -q -e PCM -d 1 -l 16 -c 2 -o 44100-PCM-16-2.wav
./swavgen sine -s 48000 -q -e PCM -d 1 -l 16 -c 2 -o 48000-PCM-16-2.wav
./swavgen sine -s 88200 -q -e PCM -d 1 -l 16 -c 2 -o 88200-PCM-16-2.wav
./swavgen sine -s 96000 -q -e PCM -d 1 -l 16 -c 2 -o 96000-PCM-16-2.wav
./swavgen sine -s 176400 -q -e PCM -d 1 -l 16 -c 2 -o 176400-PCM-16-2.wav
./swavgen sine -s 192000 -q -e PCM -d 1 -l 16 -c 2 -o 192000-PCM-16-2.wav
./swavgen sine -s 352800 -q -e PCM -d 1 -l 16 -c 2 -o 352800-PCM-16-2.wav
./swavgen sine -s 384000 -q -e PCM -d 1 -l 16 -c 2 -o 384000-PCM-16-2.wav
./swavgen sine -s 705600 -q -e PCM -d 1 -l 16 -c 2 -o 705600-PCM-16-2.wav
./swavgen sine -s 768000 -q -e PCM -d 1 -l 16 -c 2 -o 768000-PCM-16-2.wav

./swavgen sine -s 44100 -q -e PCM -d 1 -l 24 -c 2 -o 44100-PCM-24-2.wav
./swavgen sine -s 48000 -q -e PCM -d 1 -l 24 -c 2 -o 48000-PCM-24-2.wav
./swavgen sine -s 88200 -q -e PCM -d 1 -l 24 -c 2 -o 88200-PCM-24-2.wav
./swavgen sine -s 96000 -q -e PCM -d 1 -l 24 -c 2 -o 96000-PCM-24-2.wav
./swavgen sine -s 176400 -q -e PCM -d 1 -l 24 -c 2 -o 176400-PCM-24-2.wav
./swavgen sine -s 192000 -q -e PCM -d 1 -l 24 -c 2 -o 192000-PCM-24-2.wav
./swavgen sine -s 352800 -q -e PCM -d 1 -l 24 -c 2 -o 352800-PCM-24-2.wav
./swavgen sine -s 384000 -q -e PCM -d 1 -l 24 -c 2 -o 384000-PCM-24-2.wav
./swavgen sine -s 705600 -q -e PCM -d 1 -l 24 -c 2 -o 705600-PCM-24-2.wav
./swavgen sine -s 768000 -q -e PCM -d 1 -l 24 -c 2 -o 768000-PCM-24-2.wav

./swavgen sine -s 44100 -q -e PCM -d 1 -l 32 -c 2 -o 44100-PCM-32-2.wav
./swavgen sine -s 48000 -q -e PCM -d 1 -l 32 -c 2 -o 48000-PCM-32-2.wav
./swavgen sine -s 88200 -q -e PCM -d 1 -l 32 -c 2 -o 88200-PCM-32-2.wav
./swavgen sine -s 96000 -q -e PCM -d 1 -l 32 -c 2 -o 96000-PCM-32-2.wav
./swavgen sine -s 176400 -q -e PCM -d 1 -l 32 -c 2 -o 176400-PCM-32-2.wav
./swavgen sine -s 192000 -q -e PCM -d 1 -l 32 -c 2 -o 192000-PCM-32-2.wav
./swavgen sine -s 352800 -q -e PCM -d 1 -l 32 -c 2 -o 352800-PCM-32-2.wav
./swavgen sine -s 384000 -q -e PCM -d 1 -l 32 -c 2 -o 384000-PCM-32-2.wav
./swavgen sine -s 705600 -q -e PCM -d 1 -l 32 -c 2 -o 705600-PCM-32-2.wav
./swavgen sine -s 768000 -q -e PCM -d 1 -l 32 -c 2 -o 768000-PCM-32-2.wav

ffmpeg -i 176400-PCM-16-2.wav -c:a flac -sample_fmt s16 176400-PCM-16-2.flac
ffmpeg -i 192000-PCM-16-2.wav -c:a flac -sample_fmt s16 192000-PCM-16-2.flac
ffmpeg -i 352800-PCM-16-2.wav -c:a flac -sample_fmt s16 352800-PCM-16-2.flac
ffmpeg -i 384000-PCM-16-2.wav -c:a flac -sample_fmt s16 384000-PCM-16-2.flac
ffmpeg -i 44100-PCM-16-2.wav -c:a flac -sample_fmt s16 44100-PCM-16-2.flac
ffmpeg -i 48000-PCM-16-2.wav -c:a flac -sample_fmt s16 48000-PCM-16-2.flac
ffmpeg -i 88200-PCM-16-2.wav -c:a flac -sample_fmt s16 88200-PCM-16-2.flac
ffmpeg -i 96000-PCM-16-2.wav -c:a flac -sample_fmt s16 96000-PCM-16-2.flac

ffmpeg -i 176400-PCM-24-2.wav -c:a flac -sample_fmt s32 176400-PCM-24-2.flac
ffmpeg -i 192000-PCM-24-2.wav -c:a flac -sample_fmt s32 192000-PCM-24-2.flac
ffmpeg -i 352800-PCM-24-2.wav -c:a flac -sample_fmt s32 352800-PCM-24-2.flac
ffmpeg -i 384000-PCM-24-2.wav -c:a flac -sample_fmt s32 384000-PCM-24-2.flac
ffmpeg -i 44100-PCM-24-2.wav -c:a flac -sample_fmt s32 44100-PCM-24-2.flac
ffmpeg -i 48000-PCM-24-2.wav -c:a flac -sample_fmt s32 48000-PCM-24-2.flac
ffmpeg -i 88200-PCM-24-2.wav -c:a flac -sample_fmt s32 88200-PCM-24-2.flac
ffmpeg -i 96000-PCM-24-2.wav -c:a flac -sample_fmt s32 96000-PCM-24-2.flac

ffmpeg -i 176400-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 176400-PCM-32-2.flac
ffmpeg -i 192000-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 192000-PCM-32-2.flac
ffmpeg -i 352800-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 352800-PCM-32-2.flac
ffmpeg -i 384000-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 384000-PCM-32-2.flac
ffmpeg -i 44100-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 44100-PCM-32-2.flac
ffmpeg -i 48000-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 48000-PCM-32-2.flac
ffmpeg -i 88200-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 88200-PCM-32-2.flac
ffmpeg -i 96000-PCM-32-2.wav -c:a flac -sample_fmt s32 -strict experimental 96000-PCM-32-2.flac
