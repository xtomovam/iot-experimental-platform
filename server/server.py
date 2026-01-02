import socket

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

SERVER_IP = "0.0.0.0"   # listens on all network interfaces
DATA_PACKET_SIZE_B = 1 + 4 + 1 + NUM_DIMS * 2 # Device ID + sequence number + AoI + data
FEEDBACK_PACKET_SIZE_B = 1 + 4 + 1  # Device ID + sequence number + AoI

def main():
    # create TCP socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # faster restarts
        s.bind((SERVER_IP, SERVER_PORT))
        s.listen()
        print(f"Server listening on {SERVER_IP}:{SERVER_PORT}")

        while True:
            # accept a connection
            conn, addr = s.accept()
            print(f"Connected by {addr}")

            # set keepalive options
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 60)
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 10)
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            with conn:
                while True:
                    try:
                        # receive data
                        data = conn.recv(DATA_PACKET_SIZE_B)
                        if not data:
                            print("Client disconnected")
                            break
                        print(f"Received {len(data)} bytes: {data.hex()}")

                        # echo back excluding data part
                        conn.sendall(data[:FEEDBACK_PACKET_SIZE_B])

                    except Exception as e:
                        print(f"Error: {e}")
                        break

if __name__ == "__main__":
    main()
