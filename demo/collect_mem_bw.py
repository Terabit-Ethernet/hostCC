# collect_cpu_utilization.py
import subprocess
import sqlite3
import time
import os
import subprocess
import threading

CORE_NUMBER = 0  # Specify the core number you want to monitor
MAX_ROWS = 1000
MAX_MEM_BW = 46900

DATABASE = 'instance/metrics.db'

def create_database():
    if not os.path.exists(DATABASE):
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('''CREATE TABLE membw_data
                        (id INTEGER PRIMARY KEY AUTOINCREMENT,
                        timestamp INTEGER,
                        mem_bw REAL)''')
        conn.commit()
        conn.close()

def run_command(command):
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()

    if stdout:
        print("Standard Output:")
        print(stdout.decode('utf-8'))
    if stderr:
        print("Standard Error:")
        print(stderr.decode('utf-8'))

def collect_mem_bw():
    create_database()  # Create the database and table if they don't exist
    
    bash_command = "sudo taskset -c 31 /home/saksham/pcm/build/bin/pcm-memory 1 -columns=5 > logs/membw.log"
    thread = threading.Thread(target=run_command, args=(bash_command,))
    thread.start()
    time.sleep(3)
    # print(os.system(bash_command))

    while True:
        timestamp = int(time.time())
        # command = "cat logs/pcie.csv | grep \"Socket0,IIO Stack 2 - PCIe1,Part0\" | tail -n 2 | head -n 1 |  awk -F ',' '{ sum += $4/1000000000.0; n++ } END { if (n > 0) printf \"%.3f\", sum / n * 8 ; }'"
        command = "cat logs/membw.log | grep \"NODE 0 Memory\" | tail -n 2 | head -n 1 |  awk '{ sum += $6; n++ } END { if (n > 0) printf \"%.3f\", sum / n; }'"
        mem_bw = float(subprocess.check_output(command, shell=True).decode().strip())
        mem_bw_util = mem_bw/MAX_MEM_BW * 100
        print(timestamp,mem_bw_util)
        
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        # Check if the number of rows exceeds the limit
        cursor.execute('SELECT COUNT(*) FROM membw_data')
        num_rows = cursor.fetchone()[0]
        
        if num_rows >= MAX_ROWS:
            # Delete the oldest row
            cursor.execute('DELETE FROM membw_data WHERE id = (SELECT MIN(id) FROM membw_data)')
        
        # Insert the new data
        cursor.execute('INSERT INTO membw_data (timestamp, mem_bw) VALUES (?, ?)', (timestamp, mem_bw_util))
        conn.commit()
        conn.close()
        time.sleep(1)

if __name__ == "__main__":
    collect_mem_bw()
