#!/usr/bin/env python3
"""
Optional PC-side UDP-to-MIDI listener for Bard Beats Wi-Fi MIDI Bridge (port 5004).
Requires: pip install python-rtmidi
"""
import socket
import sys

try:
    import rtmidi
except ImportError:
    print("Install python-rtmidi first: pip install python-rtmidi")
    sys.exit(1)

midi_out = rtmidi.MidiOut()
ports = midi_out.get_ports()
if ports:
    midi_out.open_port(0)
    print(f"Opened MIDI output: {ports[0]}")
else:
    midi_out.open_virtual_port("Bard Beats 3DS")
    print("Opened virtual MIDI port: Bard Beats 3DS")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5004))
print("Listening for Bard Beats MIDI packets on UDP port 5004...")

while True:
    data, addr = sock.recvfrom(64)
    if len(data) == 3:
        midi_out.send_message(list(data))
