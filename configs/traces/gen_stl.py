import random

burst_len = 32*4*2
max_address_bit = 26
trace_file_name = str(burst_len) + '_read_' + str(max_address_bit) + 'bits.stl'
random_file_name = str(burst_len) + '_random_' + str(max_address_bit) + 'bits.stl'
max_address = 1 << max_address_bit

with open(trace_file_name, 'w') as trace_file:
    addr = 0;
    index = 0;
    while (index < max_address/burst_len):
        print('%d: (%d) read 0x%x' % (index, burst_len, addr), file = trace_file);
        addr += burst_len;
        index += 1;

trace_file.close();


# random address generation
random_seed = 0

# using the same seed multiple times
random_block_bit = 26
random_max_address = 1 << random_block_bit



with open(random_file_name, 'w') as random_file:
    index = 0
    block_index = 0
    for block_index in range(max_address//random_max_address):
        random.seed(random_seed)
        random_index = 0
        while (random_index < random_max_address/burst_len):
            addr = random.randint(0, random_max_address/burst_len) * burst_len + block_index * random_max_address
            print('%d: (%d) read 0x%x' % (index, burst_len, addr), file = random_file)
            random_index += 1
            index += 1
        
    
random_file.close();
