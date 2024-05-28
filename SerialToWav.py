import time
import serial
import sys
import time
import argparse
import wave
import struct

# Serial MIDI Bridge
# Ryan Kojima


parser = argparse.ArgumentParser(description = "Serial MIDI bridge")

parser.add_argument("--serial_name", type=str, required = True, help = "Serial port name. Required")
parser.add_argument("--baud", type=int, default=115200, help = "Baud rate. Default is 115200")
parser.add_argument("--output", type=str, default="output.wav", help = "Output destination. Default is output.wav")

args = parser.parse_args()

thread_running = True

# Arguments
serial_port_name = args.serial_name
serial_baud = args.baud
output_file_name = args.output

# Initialize serial port
try:
    ser = serial.Serial(serial_port_name,serial_baud)
    ser.timeout = 0.4
except serial.serialutil.SerialException:
    print("Serial port opening error.")
    sys.exit()

# Create wav file
w = wave.open(output_file_name, 'wb')
w.setnchannels(1)
w.setsampwidth(2)
w.setframerate(48000)
w.setnframes(96000)

# Watch serial input
try:
    while True:
        data = ser.read_until(b",")
        if data:
            w.writeframes(struct.pack('h', int(data[0:-1])))
        else:
            time.sleep(0.1)
except KeyboardInterrupt:
    w.close()
    sys.exit()
