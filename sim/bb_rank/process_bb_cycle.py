import sys

input_file = sys.argv[1]
interval = int(sys.argv[2])

with open(input_file, 'r') as f:
    lines = f.readlines()

line = lines[interval]

items = line.split(" ")
bb_count_map = {}

output = []
print(len(items))
for item in items:
    parts = item.split(":")[1:]
    bb = int(parts[0]) - 1
    count = int(parts[1])
    
    if bb not in bb_count_map:
        bb_count_map[bb] = 0
    bb_count_map[bb] += count

bb_cycles = open(sys.argv[3],'w+')
for key, value in bb_count_map.items():
    print(key, value)
    bb_cycles.write("r%s %s\n" % (key, value)) 
    
bb_cycles.close()