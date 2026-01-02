import serial
import time
import csv
import os

# ==== AUTO-GENERATED CONFIG START ====
# (do not edit this block manually)
SENSOR_NODE_PORT = "/dev/ttyUSB0"
SERVER_IP = "95.169.201.66"
SERVER_PORT = 5000
EXPERIMENT_NAME = "psbo/mid-1"
AGENT_TYPE_STR = "PSBO"
GOT_METRIC = "general"
READ_FROM_SERIAL = True
TIME_STEP_MS = 1000
NUM_DIMS = 1
DIM_NAMES = ["light"]
NUM_STATES = 7
STATES = [[0, 1, 2, 3, 4, 5, 6]]
NUM_AOI_BUCKETS = 7
LIGHT_BUCKETS_MAX = [585, 1170, 1755, 2340, 2925, 3510, 4095]
MAX_AOI = 60
ADV_INTERVAL_MS = 32
ADV_DURATION_MS = 96
SENSOR_NODE_ID = 1
SENSOR_NODE_NAME = "sensor_node_1"
GW_NAME = "BLE_gateway"
MAX_TRANSMISSIONS = 1
AOI_BUCKETS_MAX = [2, 8, 32, 60]
AGENT_SAVING_MS = [0, 0, 0, 422, 100]
ENERGY_WEIGHT = 1.0
GOT_WEIGHT = 1.0
DEEP_SLEEP_MJ_PER_MS = 0.004867
WAKE_UP_MJ_PER_MS = 0.19125
IDLE_MJ_PER_MS = 0.13795
SENSING_MJ_PER_MS = 0.0
ANTENNA_MJ_PER_MS = 0.50755
SENSING_INTERVAL_MS = 0
WAKE_UP_INTERVAL_MS = 69
BASE_VALUES = [[[0, 507, 637, 352, 516, 487, 340], [314, 0, 644, 308, 428, 362, 380], [462, 678, 0, 340, 434, 627, 332], [361, 592, 471, 0, 334, 400, 556], [517, 697, 582, 506, 0, 341, 478], [651, 540, 395, 521, 536, 0, 328], [312, 626, 542, 583, 469, 361, 0]]]
GROWTHS = [[[512, 536, 666, 579, 551, 679, 456], [600, 364, 626, 643, 435, 438, 691], [588, 560, 530, 327, 500, 567, 347], [515, 398, 659, 513, 526, 430, 304], [554, 658, 692, 314, 645, 679, 362], [530, 351, 687, 530, 442, 470, 335], [459, 486, 385, 365, 344, 433, 583]]]
# ==== AUTO-GENERATED CONFIG END ====

# ==== USER CODE BELOW ====

# configuration
folder = EXPERIMENT_NAME
dims = DIM_NAMES
time_step_ms = TIME_STEP_MS

# global variables for tracking environment state, energy consumption, and messages from the sensor node
step = 0
state = ({dim: {"Xproc": -1,"Xtx": -1, "Xrx": -1, "AoItx": 0, "AoIrx": 0, "AoIIrx": 0} for dim in dims})
energy_cost = 0.0
delay = 0
deep_sleep_mj_pre_ms = 0.004867
response = None

# initiate log files
def create_empty_log_files():
    # create directory for experiment logs
    if not os.path.exists(folder):
        os.makedirs(folder)

    # prepare list of files to create
    files = []
    files.append(f"{folder}/msg_log.csv")
    files.append(f"{folder}/energy_cost.csv")
    files.append(f"{folder}/delays.csv")

    for dim in dims:
        for key in state[dim].keys():
            files.append(f"{folder}/{key}['{dim}'].csv")

    # create empty files
    for file in files:
        with open(file, mode="w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([])

# log message from serial, state, and consumed energy
def log_step():
    # log message from serial
    with open(f"{folder}/msg_log.csv", mode="a") as f:
        writer = csv.writer(f)
        writer.writerow([step, response])

    # log state
    for dim in dims:
        for key in state[dim].keys():
            with open(f"{folder}/{key}[\'{dim}\'].csv", mode="a", newline="") as f:
                writer = csv.writer(f)
                writer.writerow([step, state[dim][key]])

    # log energy cost + delay
    with open(f"{folder}/energy_cost.csv", mode="a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([step, energy_cost])
    with open(f"{folder}/delays.csv", mode="a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([step, delay])

# try to get response from serial, time out at the end of time step
def wait_for_response(ser):
    response = None
    waiting_start = time.time()
    time_elapsed = 0
    while time_elapsed < time_step_ms / 1000: # wait until the end of the time step
        if ser.in_waiting > 0:
            line = ser.readline().decode(errors="ignore").strip()
            if line.startswith("SYNCH"):
                time_elapsed = 0
            if line and (line.startswith("0 ") or line.startswith("1 ")): # ignore debug messages
                response = line
                break
        time_elapsed = time.time() - waiting_start if int(step) >= 0 else 0 # do not time out on the first step
    return response

# update energy and state when device sleeps
def process_no_response():
    global state, response, energy_cost, delay

    # update state
    for dim in dims:
        state[dim]["AoItx"] += 1
        state[dim]["AoIrx"] += 1
        state[dim]["AoIIrx"] = 0 if state[dim]["Xrx"] == state[dim]["Xproc"] else state[dim]["AoIIrx"] + 1
                                                      
    # update energy cost + delay
    energy_cost = f"{deep_sleep_mj_pre_ms * time_step_ms}"
    delay = "0"

    # update response
    response = "SLEEP"

# update energy and state based on the obtained message
def process_response():
    global state, energy_cost, delay

    # if no response, assume the device is sleeping
    if response is None:
        process_no_response()
        return
    
    # parse response
    parts = response.split()
    server_response = parts[0]
    energy_cost = parts[1]
    delay = parts[2]

    # update state
    for dim in dims:
        state[dim]["Xtx"] = state[dim]["Xproc"]
        state[dim]["Xrx"] = state[dim]["Xtx"] if server_response == "1" else state[dim]["Xrx"]
        state[dim]["AoItx"] = 0
        state[dim]["AoIrx"] = 0 if server_response == "1" else state[dim]["AoIrx"] + 1
        state[dim]["AoIIrx"] = 0 if state[dim]["Xrx"] == state[dim]["Xproc"] else state[dim]["AoIIrx"] + 1

# send light-intensity bin values, process messages, write to log files
def main():
    global step, state, energy_cost, response

    # initialize log files
    create_empty_log_files()

    # initiate serial connection
    ser = serial.Serial(SENSOR_NODE_PORT, 115200)
    
    # open synthetic data file
    step = -1
    with open('synthetic_light.csv') as f:
        reader = csv.reader(f)
        next(reader) # skip header

        # main loop
        for row in reader:
            step_start_time = time.time()
            
            # parse row in synthetic data file
            step = row[0]
            for i, dim in enumerate(dims):
                state[dim]["Xproc"] = row[2*i+2]

            # send light intensity bin to the serial port
            for dim in dims:
                ser.write(f"{state[dim]['Xproc']}\n".encode())

            # wait for response
            response = wait_for_response(ser)

            # process response
            process_response()

            # log step
            log_step()

            # wait for the next time step
            while time.time() - step_start_time < time_step_ms / 1000:
                time.sleep(0.001)

if __name__ == "__main__":
    main()
