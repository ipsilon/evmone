import csv
import re
import sys

pattern = re.compile('.*PrecompileId::(\w+), (\w+)[^/]*(/(\d+))?_(mean|stddev)')

data = []

with open(sys.argv[1], newline='') as csvfile:
    reader = csv.reader(csvfile)
    for row in reader:
        name = row[0]
        r = re.match(pattern, name)
        if r:
            id = r.group(1)
            impl = r.group(2)
            count = r.group(4) if r.group(4) else '1'
            type = r.group(5)
            if type == 'mean':
                data.append([id, impl, count, row[3]])
            elif type == 'stddev':
                data[-1].append(row[3])
                # print(count, row[3])
            # print(r.group(0), r.group(1), r.group(2), r.group(3), r.group(4), r.group(5))
        # if "_mean" in name:
        #     print(', '.join(row))

for d in data:
    print(",".join(d))
