#!/usr/bin/env python

import os,time,sys,json,copy
from datetime import datetime

event = dict()
last_onset_event = dict()
event["cage"] = int(sys.argv[1])
event["type"] = str(sys.argv[2])

def get_time_since_last_event():
    current_time = datetime.now()
    previous_time = event["timestamp"]
    return (current_time - previous_time).total_seconds()

def emit_event():
    tmp = copy.deepcopy(event)
    tmp['timestamp'] = event['timestamp'].isoformat()
    print json.dumps(tmp)

try:
    while True:
        line = sys.stdin.readline().rstrip()
        if 'onset' in line:
            event["event"] = line
            event["timestamp"] = datetime.now()
            event["duration"] = 0
            sha_command = "echo \"" + str(event) + "\" | sha1sum | cut -d' ' -f1"
            event["hash"] = os.popen(sha_command).read().rstrip()
            last_onset_event = copy.deepcopy(event)
            emit_event()
        else:
            duration = get_time_since_last_event() 
            event = copy.deepcopy(last_onset_event)
            event["event"] = line
            event["duration"] = duration
            emit_event()

except KeyboardInterrupt:
    sys.stdout.flush()
    pass


