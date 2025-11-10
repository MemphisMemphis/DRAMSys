with open('512_write_29bit.stl', 'w') as file:
    addr = 0;
    index = 0;
    while (addr < 0x20000000):
        print('%d: (512) write 0x%x' % (index,addr), file = file);
        addr += 0x200;
        index += 1;

file.close();
