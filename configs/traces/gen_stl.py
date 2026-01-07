import random

# each read burst in bytes
burst_len = 32*32

# max address space / file address space / block address space
max_address_bit = 29    
file_address_bit = 29
block_address_bit = 28

if not ((max_address_bit >= file_address_bit) and (file_address_bit >= block_address_bit)) :
    exit(-1)

max_space = 1 << max_address_bit
file_space = 1 << file_address_bit
block_space = 1 << block_address_bit

for file_index in range(max_space//file_space):
    file_offset = file_index * file_space
    trace_file_name = str(burst_len) + '_read_' + str(file_address_bit) + 'bits' + str(file_index) + '.stl'
    random_file_name = str(burst_len) + '_random_' + str(file_address_bit) + 'bits' + str(file_index) + '.stl'
    with open(trace_file_name, 'w') as trace_file, open(random_file_name, 'w') as random_file:
        index = 0
        for block_index in range(file_space//block_space):
            block_offset = block_index * block_space
            random_seed = 0
            random.seed(random_seed)    
            addr = block_offset + file_offset
            block_index = 0
            for block_index in range(block_space//burst_len):
                print('%d: (%d) read 0x%x' % (index, burst_len, addr), file = trace_file);
                addr += burst_len;
                random_addr = random.randint(0, block_space/burst_len) * burst_len + block_offset + file_offset
                print_str = '%d: (%d) ' + random.choice(['read', 'write']) + ' 0x%x'
                print(print_str % (index, burst_len, random_addr), file = random_file)
                index += 1
    
    trace_file.close()
    random_file.close()

