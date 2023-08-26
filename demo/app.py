# app.py
from flask import Flask, render_template, jsonify, request
from flask_sqlalchemy import SQLAlchemy
import os
import subprocess

app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///metrics.db'
db = SQLAlchemy(app)

class CpuData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.Integer)
    utilization = db.Column(db.Float)

class PcieData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.Integer)
    pcie_bw = db.Column(db.Float)

class MembwData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.Integer)
    mem_bw = db.Column(db.Float)

class DropsData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.Integer)
    drop_rate = db.Column(db.Float)

class IioData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.Integer)
    iio_occ = db.Column(db.Float)

# Force the creation of the table
with app.app_context():
    db.create_all()

@app.route('/')
def index():
    return render_template('dashboard.html')

@app.route('/launch-mlc', methods=['POST'])
def launch_mlc():
    try:
        data = request.get_json()
        slider_value = data['slider']
        # Replace 'your_script.sh' with the actual path to your Bash script
        subprocess.run(['bash', 'launch-mlc.sh',str(slider_value)])
        return jsonify({'message': 'Script executed successfully'})
    except Exception as e:
        return jsonify({'message': 'Error executing script', 'error': str(e)})

@app.route('/kill-mlc', methods=['POST'])
def kill_mlc():
    try:
        # Replace 'your_script.sh' with the actual path to your Bash script
        subprocess.run(['bash', 'kill-mlc.sh'])
        return jsonify({'message': 'Script executed successfully'})
    except Exception as e:
        return jsonify({'message': 'Error executing script', 'error': str(e)})

@app.route('/load-hostcc', methods=['POST'])
def load_hostcc():
    try:
        data = request.get_json()
        print(data)
        slider_value = data['slider']
        host_local_response_val = int(data['hostlocalresponse'])
        network_response_val = int(data['networkresponse'])
        # Replace 'your_script.sh' with the actual path to your Bash script
        subprocess.run(['bash', 'load-hostcc.sh',str(int(float(slider_value) * 1.05)), str(host_local_response_val), str(network_response_val)])
        return jsonify({'message': 'Script executed successfully'})
    except Exception as e:
        return jsonify({'message': 'Error executing script', 'error': str(e)})

@app.route('/unload-hostcc', methods=['POST'])
def unload_hostcc():
    try:
        # Replace 'your_script.sh' with the actual path to your Bash script
        subprocess.run(['bash', 'unload-hostcc.sh'])
        return jsonify({'message': 'Script executed successfully'})
    except Exception as e:
        return jsonify({'message': 'Error executing script', 'error': str(e)})

@app.route('/get_last_n_cpu_data/<int:n>')
def get_last_n_cpu_data(n):
    data_entries = CpuData.query.order_by(CpuData.timestamp.desc()).limit(n).all()
    
    timestamps = [entry.timestamp for entry in data_entries]
    utilizations = [entry.utilization for entry in data_entries]
    
    while len(timestamps) < n:
        timestamps.insert(0, 0)
        utilizations.insert(0, 0.0)
    
    return jsonify({'timestamps': timestamps, 'utilizations': utilizations})

@app.route('/get_last_n_pcie_data/<int:n>')
def get_last_n_pcie_data(n):
    data_entries = PcieData.query.order_by(PcieData.timestamp.desc()).limit(n).all()
    
    timestamps = [entry.timestamp for entry in data_entries]
    pcie_bws = [entry.pcie_bw for entry in data_entries]
    
    while len(timestamps) < n:
        timestamps.insert(0, 0)
        pcie_bws.insert(0, 0.0)
    
    return jsonify({'timestamps': timestamps, 'pcie_bws': pcie_bws})

@app.route('/get_last_n_membw_data/<int:n>')
def get_last_n_membw_data(n):
    data_entries = MembwData.query.order_by(MembwData.timestamp.desc()).limit(n).all()
    
    timestamps = [entry.timestamp for entry in data_entries]
    mem_bws = [entry.mem_bw for entry in data_entries]
    
    while len(timestamps) < n:
        timestamps.insert(0, 0)
        mem_bws.insert(0, 0.0)
    
    return jsonify({'timestamps': timestamps, 'mem_bws': mem_bws})

@app.route('/get_last_n_drops_data/<int:n>')
def get_last_n_drops_data(n):
    data_entries = DropsData.query.order_by(DropsData.timestamp.desc()).limit(n).all()
    
    timestamps = [entry.timestamp for entry in data_entries]
    drop_rates = [entry.drop_rate for entry in data_entries]
    
    while len(timestamps) < n:
        timestamps.insert(0, 0)
        drop_rates.insert(0, 0.0)
    
    return jsonify({'timestamps': timestamps, 'drop_rates': drop_rates})

@app.route('/get_last_n_iio_data/<int:n>')
def get_last_n_iio_data(n):
    data_entries = IioData.query.order_by(IioData.timestamp.desc()).limit(n).all()
    
    timestamps = [entry.timestamp for entry in data_entries]
    iio_occs = [entry.iio_occ for entry in data_entries]
    
    while len(timestamps) < n:
        timestamps.insert(0, 0)
        iio_occs.insert(0, 0.0)
    
    return jsonify({'timestamps': timestamps, 'iio_occs': iio_occs})


if __name__ == "__main__":
    app.run(debug=True)
