/*
 * Copyright (C) 2009, 2012, 2014 David Kozub <zub at linux.fjfi.cvut.cz>
 *
 * This file is part of dotprint.
 *
 * dotprint is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dotprint is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dotprint. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <glibmm.h>
#include "EpsonPreprocessor.h"

EpsonPreprocessor::EpsonPreprocessor():
    m_InputState(InputState::InputNormal),
    m_EscapeState(EscapeState::Entered), // not used unless m_InputState is Escape
    m_FontSizeState(FontSizeState::FontSizeNormal)
{}

void EpsonPreprocessor::process(ICairoTTYProtected &ctty, uint8_t c)
{
    if (m_InputState == InputState::Escape)
        handleEscape(ctty, c);
    else
    {
        // Control codes handled here
        switch (c)
        {
        case 0x0e: // Expanded printing for one line
            ctty.StretchFont(2.0);
            m_FontSizeState = FontSizeState::SingleLineExpanded;
            break;

        case 0x14: // Cancel one-line expanded printing
            ctty.StretchFont(1.0);
            m_FontSizeState = FontSizeState::FontSizeNormal;
            break;

        case 0x0f: // Condensed printing
            ctty.StretchFont(10.0/17.0); // Change from 10 CPI to 17 CPI
            m_FontSizeState = FontSizeState::Condensed;
            break;

        case 0x12: // Cancel condensed printing
            ctty.StretchFont(1.0);
            m_FontSizeState = FontSizeState::FontSizeNormal;
            break;

        case '\r': // Carriage Return
            ctty.CarriageReturn();
            break;

        case '\n': // Line Feed
            if (m_FontSizeState == FontSizeState::SingleLineExpanded)
            {
                ctty.StretchFont(1.0);
                m_FontSizeState = FontSizeState::FontSizeNormal;
            }
            ctty.LineFeed();
            break;

        case 0x0c: // Form Feed
            ctty.NewPage();
            break;

        case 0x1b: // Escape
            m_InputState = InputState::Escape;
            m_EscapeState = EscapeState::Entered;
            break;

        default:
            ctty.append((char) c);
            break;
        }
    }
}

void EpsonPreprocessor::handleEscape(ICairoTTYProtected &ctty, char c)
{
    // Determine what escape code follows
    if (m_EscapeState == EscapeState::Entered)
    {
        switch (c)
        {
        case 0x45: // Set bold
            ctty.SetFontWeight(FontWeight::Bold);
            break;
        case 0x46: // Unset bold
            ctty.SetFontWeight(FontWeight::Normal);
            break;
        case 0x34: // Set italic
            ctty.SetFontSlant(FontSlant::Italic);
            break;
        case 0x35: // Unset italic
            ctty.SetFontSlant(FontSlant::Normal);
            break;
        case 0x2d: // Underline
            m_EscapeState = EscapeState::Underline;
            break;

        default:
            {
                int i = c;
                std::cerr << "EpsonPreprocessor::handleEscape(): ignoring unknown escape ESC 0x"
                    << std::setfill('0') << std::setw(2) << std::hex << i << std::endl;
                m_InputState = InputState::InputNormal; // Leave escape state
            }
        }

        /*
         * If still in the escape state (no change to special), assume
         * the operation has completed, and exit the escape state.
         *
         * This includes the error condition, as no change is done either.
         */
        if (m_EscapeState == EscapeState::Entered)
        {
            m_InputState = InputState::InputNormal;
        }

        // Allow font to update if it has changed.
        ctty.UseCurrentFont();

        return;
    }

    // For multi-byte escapes:
    switch (m_EscapeState)
    {
    case EscapeState::Underline: // ESC 0x2d u, turn underline off (0) or on (!0)
        //std::cerr << "escape: ESC 0x2d " << std::hex << c << std::endl;
        if (c != 0)
        {
            // TODO: Set underline
            //std::cerr << "EpsonPreprocessor::handleEscape(): Ignoring unsupported escape: SET UNDERLINE" << std::endl;
        }
        else
        {
            // TODO: Cancel underline
            //std::cerr << "EpsonPreprocessor::handleEscape(): Ignoring unsupported escape: DROP UNDERLINE" << std::endl;
        }
        m_InputState = InputState::InputNormal; // Leave escape state
        break;

    default:
        throw std::runtime_error("EpsonPreprocessor::handleEscape(): Internal error: entered unknown escape state "
            + std::to_string(static_cast<int>(m_EscapeState)));
    }
}
