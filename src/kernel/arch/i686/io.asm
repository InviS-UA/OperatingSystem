global i686_outb
global i686_inb

i686_outb:
    [bits 32]
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    
    ret

i686_inb:
    [bits 32]
    mov dx, [esp + 4]
    xor eax, eax
    in al, dx

    ret