#!/usr/bin/python3

import os
import sys
import subprocess
import time
import sqlite3
import zlib
import binascii
from hashlib import sha256
import serial
import const


def create_connection(db_file):
    conn = sqlite3.connect(db_file)
    conn.row_factory = sqlite3.Row
    return conn

def create_challenge_table(conn):
    cur = conn.cursor()
    cur.execute('''
    CREATE TABLE IF NOT EXISTS challenge (
        ID INTEGER PRIMARY KEY,
	    RFID TEXT,
        START_TS real NOT NULL,
        RESULT_TS real,
        USER TEXT NOT NULL,
        PASS TEXT NOT NULL,
        LEVEL INTEGER,
        WG_PASS TEXT,
        TOKEN TEXT,
        IPADR TEXT,
        NICKNAME TEXT
    )
    ''')
    conn.commit()

def insert_user(conn, rfid):
    cur = conn.cursor()
    cur.execute('INSERT INTO challenge(RFID, USER, PASS, LEVEL, START_TS) VALUES (?, ?, ?, ?, ?)', (rfid, get_user(rfid), get_pass(rfid), 1, time.time()))
    conn.commit()

def delete_user(conn, rfid):
    cur = conn.cursor()
    cur.execute('DELETE FROM challenge WHERE RFID=?', (rfid,))
    conn.commit()

def fetch_user(conn, rfid):
    cur = conn.cursor()
    cur.execute('SELECT * FROM challenge WHERE RFID=?', (rfid,))
    return cur.fetchone()

def update_user_level(conn, rfid, level):
    cur = conn.cursor()
    cur.execute('UPDATE challenge SET LEVEL=? WHERE RFID=?', (level, rfid))
    if level == 7:
        cur.execute('UPDATE challenge SET RESULT_TS=?', (time.time(),))
    conn.commit()

def update_scoreboard(conn, errors, result, token):
    cur = conn.cursor()
    cur.execute('INSERT INTO highscores(ts, errors, result, token) VALUES(?, ?, ?, ?)', (time.time(), errors, result, token))
    conn.commit()

def get_user(rfid):
    hash_string = sha256(rfid.encode()).hexdigest()
    user = 'u0x' + hash_string.lower()[:8]
    return user

def get_pass(rfid):
    bytes_array = bytes.fromhex(rfid)
    hash_bytes = sha256(bytes_array).hexdigest().upper()[:8]
    return hash_bytes

def required_errors(rfid):
    crc32_result = zlib.crc32(rfid.encode())
    return (crc32_result % 4) + 2

def get_duration(conn, rfid):
    hw_user = fetch_user(conn, rfid)
    if hw_user:
        try:
            return int(hw_user['RESULT_TS']/1000 - hw_user['START_TS']/1000)
        except TypeError:
            return 0
    return 0

def print_error(string):
    print('')
    print('+++++ ERROR +++++')
    print(string)
    print('----- ERROR -----')
    print('')

def wink():
    print('DEBUG: wink')
    try:
        subprocess.run(['./wink.py', '--winkings', '5'], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError:
        print_error('wink.py')

def bon_kidsgame(conn, errors, res_time):
    try:
        token = ''
        if errors == 0:
            token = binascii.hexlify(os.urandom(6)).decode()
            subprocess.run(['./bon_kidsgame.py', '--time', str(res_time), '--errors', str(errors), '--token', str(token)], capture_output=True, text=True, check=True)
            print('DEBUG: no error - token %s' % token)
        else:
            subprocess.run(['./bon_kidsgame.py', '--time', str(res_time), '--errors', str(errors)], capture_output=True, text=True, check=True)
            print('DEBUG: %d errors - token %s' % (errors, token))
        update_scoreboard(conn, errors, res_time, token)
        print('DEBUG: highscore %d, %0.4f written to database.' % (errors, res_time/1000))

    except subprocess.CalledProcessError as cpex:
        print_error('bon_kidsgame.py: %s' % cpex)

def bon_challenge_ack(conn, rfid, errors, res_time):
    try:
        subprocess.run(['./bon_challenge_ack.py', '--time', str(res_time), '--errors', str(errors)], capture_output=True, text=True, check=True)
        update_user_level(conn, rfid, 2)
        subprocess.run(['./wg-setup', get_user(rfid)], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as cpex:
        print_error('bon_challenge_ack.py: %s' % cpex)

def bon_challenge_nak(errors, res_time):
    try:
        subprocess.run(['./bon_challenge_nak.py', '--time', str(res_time), '--errors', str(errors)], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as cpex:
        print_error('bon_challenge_nak.py: %s' % cpex)

def bon_nothing_here():
    try:
        subprocess.run(['./bon_nothing_here.py'], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as cpex:
        print_error('bon_nothing_here.py: %s' % cpex)

def bon_decryption_hint():
    try:
        subprocess.run(['./bon_decryption_hint.py'], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as cpex:
        print_error('bon_decryption_hint.py: %s' % cpex)

def bon_shirt_voucher(conn, rfid):
    try:
        update_user_level(conn, rfid, 7)
        subprocess.run(['./bon_shirt_voucher.py', '--user', get_user(rfid), '--time', str(get_duration(conn, rfid))], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as cpex:
        print_error('bon_shirt_voucher.py: %s' % cpex)

def bon_only_one_shirt(rfid):
    try:
        subprocess.run(['./bon_only_one_shirt.py', '--user', get_user(rfid)], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as cpex:
        print_error('bon_only_one_shirt.py: %s' % cpex)

def unit_tests():
    rfid = '01020304'
    print('RFID: %s' % rfid)
    conn = create_connection(const.DBFILE)
    create_challenge_table(conn)
    hw_user = fetch_user(conn, rfid)
    if not hw_user:
        insert_user(conn, rfid)
        hw_user = fetch_user(conn, rfid)

    # kids game
    update_scoreboard(conn, 23, 23000, '')

    # printing tests
    bon_kidsgame(conn, 23, 23000)
    bon_challenge_ack(conn, rfid, 23, 23000)
    bon_challenge_nak(23, 23000)
    bon_nothing_here()
    bon_decryption_hint()
    bon_shirt_voucher(conn, rfid)
    bon_only_one_shirt(rfid)

    print('USER: %s' % get_user(rfid))
    print('PASS: %s' % get_pass(rfid))
    print('ERRORS: %d' % required_errors(rfid))
    print('DB-Player: %s' % hw_user['USER'])
    update_user_level(conn, rfid, 2)
    # delete_user(conn, rfid)
    conn.close()
    sys.exit(1)

def main():
    print(const.DBFILE)
    # just2bsure
    conn = create_connection(const.DBFILE)
    create_challenge_table(conn)
    conn.close()

    for arg in sys.argv:
        if arg == 'DEBUG':
            unit_tests()

    try:
        ser = serial.Serial(const.USBDEV, 115200, timeout=1)
    except serial.SerialException as serex:
        sys.stderr.write('Could not open serial port {}: {}\n'.format(ser.name, serex))
        sys.exit(1)
    print(ser.name)

    # game loop
    line = []
    while True:
        for byte in ser.read():
            line.append(chr(byte))
            if chr(byte) in ('\n', '\r'):
                conn = create_connection(const.DBFILE)
                wire_cmd = ''.join(line).strip()

                # new player
                if wire_cmd.find('N') == 0:
                    wire_cmd = wire_cmd.replace('N', '').upper()
                    # invalid player format
                    if len(wire_cmd) != 8:
                        print_error('PLAYER: WRONG FORMAT: %s' % wire_cmd)
                        ser.write(b'0\r')
                    else:
                        print('DEBUG: Player: %s' % wire_cmd)
                        hw_user = fetch_user(conn, wire_cmd)
                        if hw_user:
                            rfid = hw_user['RFID']
                            level = hw_user['LEVEL']
                            print('Player %s - Level %d' % (rfid, level))
                            if level == 1:
                                ser.write(b'1\r')
                            elif level == 2:
                                bon_nothing_here()
                                ser.write(b'0\r')
                            elif level == 3:
                                bon_decryption_hint()
                                ser.write(b'0\r')
                            elif level == 4:
                                bon_nothing_here()
                                ser.write(b'0\r')
                            elif level == 5:
                                bon_nothing_here()
                                ser.write(b'0\r')
                            elif level == 6:
                                bon_shirt_voucher(conn, rfid)
                                wink()
                                ser.write(b'0\r')
                            elif level == 7:
                                bon_only_one_shirt(rfid)
                                ser.write(b'0\r')
                        else:
                            insert_user(conn, wire_cmd)
                            print("Player created - Level 1")
                            ser.write(b'1\r')

                # got result
                if wire_cmd.find('R') == 0:
                    wire_cmd = wire_cmd.replace('R', '').upper()
                    csv_vals = wire_cmd.split(',')
                    if len(csv_vals) != 3:
                        print_error('INVALID RESULT: %s' % wire_cmd)
                        ser.write(b'NACK\r')
                    # valid game
                    else:
                        res_uid = csv_vals[0]
                        res_errors = int(csv_vals[1])
                        res_time = float(csv_vals[2])

                        # challenge player
                        if res_uid != '00000000':
                            hw_user = fetch_user(conn, res_uid)
                            if hw_user:
                                print('DEBUG: user found required_errors = %d' % required_errors(res_uid))
                                if res_errors == required_errors(res_uid):
                                    print('DEBUG: user found - required_errors = %d' % required_errors(res_uid))
                                    bon_challenge_ack(conn, hw_user['RFID'], res_errors, res_time)
                                    ser.write(b'ACK\r')
                                else:
                                    print('DEBUG: %s - wrong #errors: %d vs. %d'  % (res_uid, res_errors, required_errors(res_uid)))
                                    bon_challenge_nak(res_errors, res_time)
                                    ser.write(b'ACK\r')
                            else:
                                print_error('User %s not found' % res_uid)

                        # kids game
                        else:
                            print('Kids game finished with\n%d errors, time %0.4f' % (res_errors, res_time/1000))
                            bon_kidsgame(conn, res_errors, res_time)
                            ser.write(b'ACK\r')

                # cleanup
                conn.close()
                line = []
                break

    ser.close()


#
# do something (aka main)
#
if __name__ == '__main__':
    main()
