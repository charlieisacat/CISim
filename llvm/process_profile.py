
import sys

input_file = sys.argv[1]
output_file = sys.argv[2]

def format(line):
    parts = line.split(":")
    new_line = parts[0] + ":" + ":".join(parts[2:]).strip()
    
    return new_line

def increment_bb_name(line):
    parts = line.split(" ")
    for i in range(len(parts)):
        parts_2 = (parts[i]).split(":") 
        parts_2[1] = str(int(parts_2[1]) + 1)
        
        part = ":".join(parts_2) 
        parts[i] = part
    
    return " ".join(parts)

with open(input_file, 'r') as f:
    lines = f.readlines()

with open(output_file, 'w') as f:
    for line in lines:
        if line[0] == 'T':
            format_line = format(line)
            # print(format_line)
            processed_line = increment_bb_name(format_line)
            print(processed_line)
            f.write(processed_line + '\n')
        elif line[0] == 'S':
            bb = line.strip().split('=')[-1]
            print(bb)