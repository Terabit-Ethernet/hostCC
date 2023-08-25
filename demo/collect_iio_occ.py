# collect_cpu_utilization.py
import subprocess
import sqlite3
import time
import os

CORE_NUMBER = 0  # Specify the core number you want to monitor
MAX_ROWS = 1000

DATABASE = 'instance/metrics.db'

def create_database():
    if not os.path.exists(DATABASE):
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('''CREATE TABLE iio_data
                        (id INTEGER PRIMARY KEY AUTOINCREMENT,
                        timestamp INTEGER,
                        iio_occ REAL)''')
        conn.commit()
        conn.close()

def collect_drop_rates():
    create_database()  # Create the database and table if they don't exist

    while True:
        timestamp = int(time.time())
        command = f"sudo ./collect_iio_occ_ptile"
        iio_occ = float(subprocess.check_output(command, shell=True).decode().strip())
        
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        # Check if the number of rows exceeds the limit
        cursor.execute('SELECT COUNT(*) FROM iio_data')
        num_rows = cursor.fetchone()[0]
        
        if num_rows >= MAX_ROWS:
            # Delete the oldest row
            cursor.execute('DELETE FROM iio_data WHERE id = (SELECT MIN(id) FROM iio_data)')
        
        # Insert the new data
        cursor.execute('INSERT INTO iio_data (timestamp, iio_occ) VALUES (?, ?)', (timestamp, iio_occ))
        conn.commit()
        conn.close()

        time.sleep(1)

if __name__ == "__main__":
    collect_drop_rates()
