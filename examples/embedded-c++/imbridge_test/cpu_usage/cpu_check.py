from psutil import cpu_percent

for _ in range(500):
    print(cpu_percent(interval=0.01))