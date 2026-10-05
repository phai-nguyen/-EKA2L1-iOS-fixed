void ARMul_State::ChangePrivilegeMode(std::uint32_t new_mode) {
    if (Mode == new_mode)
        return;

    if (new_mode != USERBANK) {
        switch (Mode) {
        case SYSTEM32MODE: // Shares registers with user mode
        case USER32MODE:
            Reg_usr[0] = Reg[13];
            Reg_usr[1] = Reg[14];
            break;
        case IRQ32MODE:
            Reg_irq[0] = Reg[13];
            Reg_irq[1] = Reg[14];
            Spsr[IRQBANK] = Spsr_copy;
            break;
        case SVC32MODE:
            Reg_svc[0] = Reg[13];
            Reg_svc[1] = Reg[14];
            Spsr[SVCBANK] = Spsr_copy;
            break;
        case ABORT32MODE:
            Reg_abort[0] = Reg[13];
            Reg_abort[1] = Reg[14];
            Spsr[ABORTBANK] = Spsr_copy;
            break;
        case UNDEF32MODE:
            Reg_undef[0] = Reg[13];
            Reg_undef[1] = Reg[14];
            Spsr[UNDEFBANK] = Spsr_copy;
            break;
        case FIQ32MODE:
            std::copy(Reg.begin() + 8, Reg.end() - 1, Reg_firq.begin());
            Spsr[FIQBANK] = Spsr_copy;
            break;
        }

        switch (new_mode) {
        case USER32MODE:
            Reg[13] = Reg_usr[0];
            Reg[14] = Reg_usr[1];
            Bank = USERBANK;
            break;
        case IRQ32MODE:
            Reg[13] = Reg_irq[0];
            Reg[14] = Reg_irq[1];
            Spsr_copy = Spsr[IRQBANK];
            Bank = IRQBANK;
            break;
        case SVC32MODE:
            Reg[13] = Reg_svc[0];
            Reg[14] = Reg_svc[1];
            Spsr_copy = Spsr[SVCBANK];
            Bank = SVCBANK;
            break;
        case ABORT32MODE:
            Reg[13] = Reg_abort[0];
            Reg[14] = Reg_abort[1];
            Spsr_copy = Spsr[ABORTBANK];
            Bank = ABORTBANK;
            break;
        case UNDEF32MODE:
            Reg[13] = Reg_undef[0];
            Reg[14] = Reg_undef[1];
            Spsr_copy = Spsr[UNDEFBANK];
            Bank = UNDEFBANK;
            break;
        case FIQ32MODE:
            std::copy(Reg_firq.begin(), Reg_firq.end(), Reg.begin() + 8);
            Spsr_copy = Spsr[FIQBANK];
            Bank = FIQBANK;
            break;
        case SYSTEM32MODE: // Shares registers with user mode.
            Reg[13] = Reg_usr[0];
            Reg[14] = Reg_usr[1];
            Bank = SYSTEMBANK;
            break;
        }

        // Set the mode bits in the APSR
        Cpsr = (Cpsr & ~Mode) | new_mode;
        Mode = new_mode;
    }
}
