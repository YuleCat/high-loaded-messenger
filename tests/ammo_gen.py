#!/usr/bin/python
# -*- coding: utf-8 -*-

import json
import random
import string
import sys

def make_ammo(method, url, headers, case, body, add=0):
    """ makes phantom ammo """
    #http request w/o entity body template
    req_template = (
          "%s %s HTTP/1.1\r\n"
          "%s\r\n"
          "\r\n"
    )

    #http request with entity body template
    req_template_w_entity_body = (
          "%s %s HTTP/1.1\r\n"
          "%s\r\n"
          "Content-Length: %d\r\n"
          "\r\n"
          "%s\r\n"
    )

    if not body:
        req = req_template % (method, url, headers)
    else:
        req = req_template_w_entity_body % (method, url, headers, len(body), body)

    #phantom ammo template
    ammo_template = (
        "%d %s\n"
        "%s"
    )

    return ammo_template % (len(req) + add, case, req)

def generate_message(max_length):
    length = random.randint(1, max_length)
    allowed = string.ascii_letters + string.digits + ' '
    res = [random.choice(allowed) for _ in range(length)]
    return ''.join(res)

def generate_queries(filename, lines_count, users_number):
    f = open(filename, 'w')
    for _ in range(lines_count):
        current_user_number = str(random.randint(1, users_number))
        while True:
            friend_number = str(random.randint(1, users_number))
            if friend_number != current_user_number:
                break
        
        headers = """Content-type: application/json
Host: 192.168.0.104:18080
Cookie: username=User{}
Connection: close""".format(current_user_number)

        if random.randint(0, 1) == 0:
            time_stamp = random.randint(0, 10**3)
            print(make_ammo('GET', '/chat/load_messages/{}/{}'.format("User" + friend_number, time_stamp), headers, 'the_only_tag', '', 4), file=f)
        else:
            message = generate_message(100)
            message_info = {
                'friend_name': "User" + friend_number,
                'message': message,
            }
            body = json.dumps(message_info)
            print(make_ammo('POST', '/chat/send', headers, 'the_only_tag', body, 5), file=f)
    f.close()

def generate_simple_queries(lines_count):
    f = open('ammo.txt', 'w')
    for _ in range(lines_count):        
        headers = """Content-type: application/json
Host: 192.168.0.104:18080
Connection: close"""
        print(make_ammo('GET', '/', headers, 'the_only_tag', '', 4), file=f)
    f.close()

if __name__ == "__main__":
    generate_queries('ammo.txt', 2 * 10**4, 10)