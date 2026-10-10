OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;

    init_text PT_LOAD;
    init_rodata PT_LOAD;
    init_data PT_LOAD;

    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0xffffffff80000000;

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __limine_requests_start = .;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __limine_requests_page_count = (. - __limine_requests_start) / CONSTANT(MAXPAGESIZE);
    __init_text_start = .;

    .init.text : {
        */boot.a:*(.text .text.*)
        *(.init.text .init.text.*)
    } :init_text

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __init_text_page_count = (. - __init_text_start) / CONSTANT(MAXPAGESIZE);
    __init_rodata_start = .;

    .init.rodata : {
        */boot.a:*(.rodata .rodata.*)
        *(.init.rodata .init.rodata.*)
    } :init_rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __init_rodata_page_count = (. - __init_rodata_start) / CONSTANT(MAXPAGESIZE);
    __init_data_start = .;

    .init.data : {
        */boot.a:*(.data .data.*)
        *(.init.data .init.data.*)
    } :init_data

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __init_data_page_count = (. - __init_data_start) / CONSTANT(MAXPAGESIZE);
    __text_start = .;

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __text_page_count = (. - __text_start) / CONSTANT(MAXPAGESIZE);
    __rodata_start = .;

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __rodata_page_count = (. - __rodata_start) / CONSTANT(MAXPAGESIZE);
    __late_rodata_start = .;

    .late.rodata : {
        *(.late.rodata .late.rodata.*)
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __late_rodata_page_count = (. - __late_rodata_start) / CONSTANT(MAXPAGESIZE);
    __data_start = .;

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    __data_page_count = (. - __data_start) / CONSTANT(MAXPAGESIZE);

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}
