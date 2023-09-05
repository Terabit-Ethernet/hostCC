# collect_cpu_utilization.py
import subprocess
import sqlite3
import time
import os
# from shared_utils import lat_app_req_size

CORE_NUMBER = 0  # Specify the core number you want to monitor
MAX_ROWS = 1000

DATABASE = 'instance/metrics.db'

def create_database():
    if not os.path.exists(DATABASE):
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('''CREATE TABLE latency_data
                        (id INTEGER PRIMARY KEY AUTOINCREMENT,
                        timestamp INTEGER,
                        p99_lat REAL,
                        p999_lat REAL,
                        p9999_lat REAL)''')
        conn.commit()
        conn.close()

def collect_latency():
    create_database()  # Create the database and table if they don't exist

    while True:
        timestamp = int(time.time())
        # Specify the value of req_size
        # global lat_app_req_size
        # req_size = lat_app_req_size  # You can change this value as needed
        # req_size = 128
        with open('req-value.txt', 'r') as file:
            req_size = int(file.read())
        # req_size = 32000  # You can change this value as needed
        
        # Replace the command line with the new command including req_size
        command = f"taskset -c 16 netperf -H 192.168.10.121 -t TCP_RR -l 5 -p 5050 -j MIN_LATENCY -f g -- -r {req_size},{req_size} -o P99_LATENCY,P999_LATENCY,P9999_LATENCY | awk 'END{{print}}'"
        
        latency_output = subprocess.check_output(command, shell=True).decode().strip()
        
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        # Check if the number of rows exceeds the limit
        cursor.execute('SELECT COUNT(*) FROM latency_data')
        num_rows = cursor.fetchone()[0]
        
        if num_rows >= MAX_ROWS:
            # Delete the oldest row
            cursor.execute('DELETE FROM latency_data WHERE id = (SELECT MIN(id) FROM latency_data)')

        values = latency_output.split(',')
        print(req_size,timestamp,values)

        if len(values) == 3:
            p99_lat, p999_lat, p9999_lat = values
        else:
            p99_lat = 0
            p999_lat = 0
            p9999_lat = 0
        cursor.execute('INSERT INTO latency_data (timestamp, p99_lat, p999_lat, p9999_lat) VALUES (?, ?, ?, ?)', (timestamp, p99_lat, p999_lat, p9999_lat))
        conn.commit()
       

        
        conn.close()

        time.sleep(1)

if __name__ == "__main__":
    collect_latency()
