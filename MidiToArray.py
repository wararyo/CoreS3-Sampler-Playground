# MIDIファイルから曲の配列を生成します
# 配列は下記の構造他eからなります
# {曲始めからの経過サンプル数, ステータス, データ1, データ2}
# ステータス
#   0x9n: ノートオン (nはチャンネル)
#   0x8n: ノートオフ (nはチャンネル)
#   0xFF: メタイベント
# ノートオン/ノートオフの場合のデータ
#   データ1: ノートナンバー
#   データ2: ベロシティ
# メタイベントの場合のデータ
#   0x2F, 0x00: 曲の終わり

import mido
import argparse
import time

parser = argparse.ArgumentParser(description = "Serial to wav converter")

parser.add_argument("--input", type=str, default="input.mid", help = "Input midi file. Default is output.wav")
parser.add_argument("--output", type=str, default="song.cpp", help = "Output destination. Default is song.c")
parser.add_argument("--sample_rate", type=int, default=48000, help = "Sample rate. Default is 48000")
parser.add_argument("--song_name", type=str, default="song", help = "Song name. Default is 48000")

args = parser.parse_args()

# Arguments
input_file_name = args.input
output_file_name = args.output
sample_rate = args.sample_rate
song_name = args.song_name

midi = mido.MidiFile(input_file_name)
output_file = open(output_file_name, mode='w')

tick_per_beat = midi.ticks_per_beat
accumulated_time = 0
tempo = 500000 # MIDIのテンポはマイクロ秒/ビートなのでBPM120は500000

output_file.writelines([
    "#pragma once\n",
    "\n",
    "#include <MidiMessage.h>\n",
    "\n",
    "typedef struct MidiMessage m;\n",
    "\n",
    f"extern const struct MidiMessage {song_name}[] = {{\n"
    ])

for msg in midi.merged_track:
    accumulated_time += msg.time / tick_per_beat * tempo / 1000000 * sample_rate
    if msg.is_meta:
        if msg.type == 'set_tempo':
            tempo = msg.tempo
        elif msg.type == 'end_of_track':
            accumulated_time += sample_rate # 曲終わりに1秒の余韻を設ける
            output_file.write(f"m{{{round(accumulated_time)},0x{msg.hex(',0x')}}}\n")
            break
    else:
        output_file.write(f"m{{{round(accumulated_time)},0x{msg.hex(',0x')}}},\n")

output_file.write("};")
output_file.close()