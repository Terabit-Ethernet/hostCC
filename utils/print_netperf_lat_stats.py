import pandas as pd
INPUT_FILE = 'netserver.log'
df = pd.read_csv(INPUT_FILE,header=None)
df = df[df[0] > 0]
print(df[0].describe(percentiles=[0.5, 0.9, 0.99, 0.999, 0.9999]))