import sys

input_file = sys.argv[1]
interval = int(sys.argv[2])

with open(input_file, 'r') as f:
    lines = f.readlines()

line = lines[interval]

items = line.split(" ")
bb_count_map = {}

bblist = open(sys.argv[3],'w+')
weights = open(sys.argv[4],'w+')
th = float(sys.argv[5])
start = float(sys.argv[6])
end = float(sys.argv[7])
output = []

total_count = 0
target_total_count = 0
print(len(items))
for item in items:
    parts = item.split(":")[1:]
    bb = int(parts[0]) - 1
    count = int(parts[1])
    
    if bb not in bb_count_map:
        bb_count_map[bb] = 0
    bb_count_map[bb] += count
    total_count += count
    
    if bb >= start and bb <= end:
        target_total_count += count

sorted_dict = dict(sorted(bb_count_map.items(), key=lambda item: item[1], reverse=True))

wtotal = 0.0
print(len(sorted_dict))
for key, value in sorted_dict.items():
    if key < start or key > end:
        continue
    ratio = value / target_total_count
    
    wtotal += ratio
    
    weights.write("r" + str(key)) 
    weights.write(" %.10f" % ratio) 
    weights.write(" %.10f" % ratio) 
    weights.write(" %d\n" % value) 
    
    bblist.write("r"+str(key)+"\n")
    
    if wtotal >= th:
        break
    
bblist.close()
weights.close()
