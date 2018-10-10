#!/usr/bin/env python2
#ps3 wireless controller interface by Robin Newman July 2017
#converts controller inputs to OSC messages
#which can be output to Sonic Pi 3, running on the local computer, or on an external machine
#rtested with ps3 "afterglow" wireless controller, dongle in Pi usb socket
#needs sudo apt-get install joystick after sudo apt-get update
#Version 2. Modified to ensure clean exit on Ubuntu

import subprocess,sys,pygame
from signal import pause
import argparse
import mido
from time import sleep
import os

os.environ["SDL_VIDEODRIVER"] = "dummy"

pygame.init()
pygame.joystick.init()
clock=pygame.time.Clock()
ps3 = pygame.joystick.Joystick(0)

ps3.init()

def print_ports(heading, port_names):
    print(heading)
    for name in port_names:
        print("    '{}'".format(name))
    print()

class BigMidiInteger:
    """Some messages use 14 bit values, these need to be spit down to
    msb and lsb before being sent."""
    def __init__(self, value):
        if 0 <= value <= 2 ** 14:
            self.msb = value // 2 ** 7
            self.lsb = value % 2 ** 7
        else:
            raise ValueError(
                'Invalid midi data value: {}'.format(value),
                'A midi datavalue must be an integer between0'
                ' and {}'.format(2 ** 14))

    def __repr__(self):
        return '<BigMidiInteger: lsb={}, msb={}>'.format(self.lsb, self.msb)

# NRPN 14bit midi can be 0 to 16,383
def send_14bitMidi(port, control, value):
    nrpn = BigMidiInteger(value)
    msbMsg = mido.Message("control_change", control=control, value=nrpn.msb)
    lsbMsg = mido.Message("control_change", control=control+32, value=nrpn.lsb)
    port.send(msbMsg)
    port.send(lsbMsg)

def normalizeTo14bit(number, minValue, maxValue):
    oldRange = abs(minValue - maxValue)
    newMin = 0
    newMax = 16383
    newRange = newMax - newMin
    newValue = (((number - minValue) * newRange) / oldRange) + newMin
    return int(round(newValue))

def control(portname):
 gate=0.1
 buttonState= [0] * 18
 joystickStates = [0.0,0.0,0.0,0.0] # UpDown L, LeftRight L, UpDown R, LeftRight R
 with mido.open_output(portname, autoreset=True) as port:
     print('Using {}'.format(port))
     print ("ps3.py: Python ps3-> MIDI interface")
     print ("Specify external Port with  ./ps3.py  [PORTNAME] on command line")
     print ("Ctrl-C to exit")

     while True:
        try:
            pygame.event.pump()

            lud=ps3.get_axis(1)
            llr=ps3.get_axis(0)
            rud=ps3.get_axis(3)
            rlr=ps3.get_axis(2)

            if abs(rud) > gate and joystickStates[0] != rud:
                v = normalizeTo14bit(rud * -1, -1.0, 1.0)
                print("rud ", rud, v)
                joystickStates[0] = rud
                send_14bitMidi(port, 10, v)
            elif abs(rud) <= gate and joystickStates[0] != 0.0:
                print("rud ", 0)
                joystickStates[0] = 0.0
                send_14bitMidi(port, 10, 8192)

            if abs(lud) > gate and joystickStates[1] != lud:
                v = normalizeTo14bit(lud * -1, -1.0, 1.0)
                print("lud ", lud, v)
                joystickStates[1] = lud
                send_14bitMidi(port, 11, v)
            elif abs(lud) <= gate and joystickStates[1] != 0.0:
                print("lud ", 0)
                joystickStates[1] = 0.0
                send_14bitMidi(port, 11, 8192)

            if abs(rlr) > gate and joystickStates[2] != rlr:
                v = normalizeTo14bit(rlr, -1.0, 1.0)
                print("rlr ", rlr, v)
                joystickStates[2] = rlr
                send_14bitMidi(port, 12, v)
            elif abs(rlr) <= gate and joystickStates[2] != 0.0:
                print("rlr ", 0)
                joystickStates[2] = 0.0
                send_14bitMidi(port, 12, 8192)

            if abs(llr) > gate and joystickStates[3] != llr:
                v = normalizeTo14bit(llr, -1.0, 1.0)
                print("llr ", llr, v)
                joystickStates[3] = llr
                send_14bitMidi(port, 13, v)
            elif abs(llr) <= gate and joystickStates[3] != 0.0:
                print("llr ", 0)
                joystickStates[3] = 0.0
                send_14bitMidi(port, 13, 8192)

            for x in xrange(0, 17):
                b=ps3.get_button(x)
                if b>0:
                    v=int(round(b*127))
                    if buttonState[x] != v:
                        print("button:", x, "sendMidiValue:", v, "fromInputValue:", b)
                        msg = mido.Message("control_change", control=70+x, value=v)
                        port.send(msg)
                        buttonState[x] = v
                else:
                    if buttonState[x] != 0:
                        print("button:", x, "sendMidiValue:", 0)
                        buttonState[x] = 0.0
                        msg = mido.Message("control_change", control=70+x, value=0)
                        port.send(msg)

            # hat=ps3.get_hat(0)
            # if hat[0]!=0 and hat[1]!=0:
            #     print("hat ", hat)
                # msg = mido.Message("control_change", control=73, value)
                # port.send(msg)

            # clock.tick(10)
            # subprocess.call("clear")
        except KeyboardInterrupt:
            print("\nExiting")
            sys.exit()

if __name__=="__main__":
    parser = argparse.ArgumentParser()
    #This arg gets the midi port to use.
    parser.add_argument("--portname",
    default="f_midi", help="MIDI Port to export to")
    args = parser.parse_args()
    portname = args.portname
    print("PS3 to MIDI")
    print_ports('Output Ports:', mido.get_output_names())
    sleep(2)
    control(portname)
