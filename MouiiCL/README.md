# MouiiCL

MouiiCL is a user mode client for the **MouClassInputInjection** driver which allows users to inject mouse button input and mouse movement input via text commands.

## Usage

1. Enable test signing on the host machine.
2. Load the MouClassInputInjection driver.
3. Execute MouiiCL.exe.

## Commands

**init**

    Usage:
        init

    Description:
        Initializes the mouse device stack context required by the kernel input
        injection interface. This command must be invoked and succeed before the
        input injection commands can be used.

---

**button**

    Usage:
        button process_id button_flag(hex) button_data(hex)

    Description:
        Inject mouse button data in the specified process context. The button data
        is validated, converted into a single mouse input data packet, and injected
        into the specified process.

    Parameters:
        process_id - The process id of the process context in which the input
            injection occurs.

        button_flag (hex) - The system defined value of the button input:

            0x001   Left mouse button down.         (Mouse button 1)
            0x002   Left mouse button up.           (Mouse button 1)
            0x004   Right mouse button down.        (Mouse button 2)
            0x008   Right mouse button up.          (Mouse button 2)
            0x010   Middle mouse button down.       (Mouse button 3)
            0x020   Middle mouse button up.         (Mouse button 4)
            0x040   Mouse button 4 down.
            0x080   Mouse button 4 up.
            0x100   Mouse button 5 down.
            0x200   Mouse button 5 up.
            0x400   Vertical mouse wheel scroll.
            0x800   Horizontal mouse wheel scroll.

        button_data (hex) - The mouse wheel delta for scrolling. This value is only
            valid if 'button_flag' specifies a mouse wheel scroll value.

    Example:
        The following command injects a middle mouse button press into the process
        whose process id is 1234:

            button 1234 0x10 0

---

**move**

    Usage:
        move process_id indicator_flags(hex) x y

    Description:
        Inject mouse movement data in the specified process context. The movement
        data is validated, converted into a single mouse input data packet, and
        injected into the specified process.

    Parameters:
        process_id - The process id of the process context in which the input
            injection occurs.

        indicator_flags (hex) - The system defined mouse indicator flags:

            0x000   'x' and 'y' specify relative mouse movement.
            0x001   'x' and 'y' specify absolute mouse movement.
            0x002   The coordinates are mapped to the virtual desktop.
            0x004   Requery for mouse attributes. (Untested)
            0x008   Do not coalesce WM_MOUSEMOVEs. (Untested)
            0x100   MOUSE_TERMSRV_SRC_SHADOW. (Restricted/untested)

        x - The motion delta to be applied in the x direction.

        y - The motion delta to be applied in the y direction.

    Example:
        The following command injects a movement packet which moves the mouse left
        by 3 relative units and up by 10 relative units:

            move 1234 0 -3 10

---

**click**

    Usage:
        click process_id button(hex) release_delay

    Description:
        Inject a mouse button press into the specified process then inject the
        corresponding mouse button release after the specified delay.

    Parameters:
        process_id - The process id of the process context in which the input
            injection occurs.

        button (hex) - One of the following system defined values:

            0x001   Left mouse button.      (Mouse button 1)
            0x004   Right mouse button.     (Mouse button 2)
            0x010   Middle mouse button.    (Mouse button 3)
            0x040   Mouse button 4.
            0x100   Mouse button 5.

        release_delay - The approximate duration in milliseconds between when the
            button press is injected and when the button release is injected.

    Example:
        The following command injects a left mouse button click where the button is
        activated (i.e., held down) for approximately 777 milliseconds.

            click 1234 0x1 777

---

**pid**

    Usage:
        pid process_name

    Description:
        Query the process id of all processes matching the specified name.

    Parameters:
        process_name - The target process name including file extension.

    Example:
        pid calc.exe

---

**help**

    Usage:
        help

    Description:
        Displays the list of supported commands.

---

**exit**

    Usage:
        exit

    Description:
        Terminate the MouiiCL session.

## Notes

* The debug configuration uses the multi-threaded debug runtime library to reduce library requirements.
