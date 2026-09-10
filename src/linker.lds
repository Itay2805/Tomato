OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0xffffffff80000000;

    /*
     * Define a section to contain the Limine requests and assign
     * it to its own PHDR
     */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));
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
