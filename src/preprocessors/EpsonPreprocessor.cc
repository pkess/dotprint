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
{
    // TODO: set this only with conditional
    m_ExpandedPrintingNoStretch = true;
    m_NewlineCountNewPage = 11;
    m_NewlineConsec = 0;
    m_FirstPage = true;
}

void EpsonPreprocessor::emptyCrLf(ICairoTTYProtected &ctty)
{
    if (m_NewlineConsec >= m_NewlineCountNewPage)
    {
        if (m_FirstPage)
        {
            m_FirstPage = false;
        }
        else
        {
            ctty.NewPage();
        }
        m_NewlineConsec = m_NewlineCountNewPage;
        std::cout << "new page" << std::endl;
    }
    for (int i = 0; i < m_NewlineConsec; ++i)
    {
        std::cout << "Print newline " << m_NewlineConsec << std::endl;
        ctty.CarriageReturn();
        ctty.LineFeed();
    }
    m_NewlineConsec = 0;
}

void EpsonPreprocessor::process(ICairoTTYProtected &ctty, uint8_t c)
{
    if (m_InputState == InputState::Escape)
    {
        handleEscape(ctty, c);
        //ctty.appendNote(c);
    }
    else
    {
        // Control codes handled here
        switch (c)
        {
        case '\r': // Carriage Return
            if (m_DoCr)
            {
                // Double cr: output first
                emptyCrLf(ctty);
                ctty.CarriageReturn();
            }
            else
            {
                m_DoCr = true;
            }
            break;

        case '\n': // Line Feed
            if (m_DoCr)
            {
                // CR LF detected
                m_DoCr = false;
                ++m_NewlineConsec;
                std::cout << "Assemble newline " << m_NewlineConsec << std::endl;
            }
            else
            {
                // LF without CR before: Empty cr lf buffer and print newline
                emptyCrLf(ctty);
                ctty.LineFeed();
            }

            if (m_FontSizeState == FontSizeState::SingleLineExpanded)
            {
                //ctty.StretchFont(1.0);
                m_FontSizeState = FontSizeState::FontSizeNormal;
            }
            break;
        default:
            emptyCrLf(ctty);
            if (m_DoCr)
            {
                // Output cr without following lf
                ctty.CarriageReturn();
                m_DoCr = false;
            }
            // Control codes handled here
            switch (c)
            {
            case 0x0e: // Expanded printing for one line
                m_ExpandedPrintingEnabled = true;
                m_ExpandedPrintingChars = 0;
                if (!m_ExpandedPrintingNoStretch)
                {
                    ctty.StretchFont(2.0);
                }
                else
                {
                    ctty.SetFontWeight(FontWeight::Bold);
                    ctty.UseCurrentFont();
                }
                m_FontSizeState = FontSizeState::SingleLineExpanded;
                break;

            case 0x14: // Cancel one-line expanded printing
                if (m_ExpandedPrintingEnabled)
                {
                    if (m_ExpandedPrintingNoStretch)
                    {
                        for (int i = 0; i < m_ExpandedPrintingChars; ++i)
                        {
                            ctty.append((gunichar) ' ');
                        }
                    }
                    m_ExpandedPrintingEnabled = false;
                    ctty.SetFontWeight(FontWeight::Normal);
                    ctty.UseCurrentFont();
                }
                else
                {
                    //ctty.StretchFont(1.0);
                }
                m_FontSizeState = FontSizeState::FontSizeNormal;
                break;

            case 0x0f: // Condensed printing
                //ctty.StretchFont(10.0/17.0); // Change from 10 CPI to 17 CPI
                m_FontSizeState = FontSizeState::Condensed;
                break;

            case 0x12: // Cancel condensed printing
                //ctty.StretchFont(1.0);
                m_FontSizeState = FontSizeState::FontSizeNormal;
                break;

            case 0x0c: // Form Feed
                ctty.NewPage();
                break;

            case 0x1b: // Escape
                m_InputState = InputState::Escape;
                m_EscapeState = EscapeState::Entered;
                break;

            default:
                if (m_ExpandedPrintingEnabled)
                {
                    m_ExpandedPrintingChars += 1;
                }
                ctty.append((char) c);
                break;
            }
        }
    }
}

void EpsonPreprocessor::handleEscape(ICairoTTYProtected &ctty, uint8_t c)
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
        case 0x2a: // '*': Draw Graphics
            m_GraphicAssembledBytes = 0;
            m_EscapeState = EscapeState::DrawGraphics;
            break;
        case 0x2d: // Underline
            m_EscapeState = EscapeState::Underline;
            break;
        case 0x33: // '3': Set n/180-inch line spacing
            m_EscapeState = EscapeState::SetLineSpacing;
            break;
        case 0x40: // '@': Initialize
            // TODO: add init for Cairo here
            m_InputState = InputState::InputNormal; // Leave escape state
            break;
        case 0x44: // 'D': Set horizontal tabs
            m_EscapeState = EscapeState::SetTabWidth;
            break;
        case 0x78: // 'x': Select LQ or draft
            m_EscapeState = EscapeState::SelectQuality;
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

    case EscapeState::SetLineSpacing:
        {
            int nrTabs = (unsigned int) c;
            double spacing = ((double) nrTabs) / 180.0;
            ctty.SetLineSpacing(spacing);
        }
        m_InputState = InputState::InputNormal;
        break;

    case EscapeState::SetTabWidth: // Set width of tabs
        if (0 == c)
        {
            m_InputState = InputState::InputNormal; // Leave escape state
        }
        else
        {
            int nrTabs = (unsigned int) c;
            ctty.SetTabWidth(nrTabs);
        }
        break;

    case EscapeState::SelectQuality:
        // TODO: select quality?
        m_InputState = InputState::InputNormal;
        break;

    case EscapeState::DrawGraphics: // Insert tabs in text
        handleGraphics(ctty, c);
        break;

    default:
        throw std::runtime_error("EpsonPreprocessor::handleEscape(): Internal error: entered unknown escape state "
            + std::to_string(static_cast<int>(m_EscapeState)));
    }
}

void EpsonPreprocessor::handleGraphics(ICairoTTYProtected &ctty, uint8_t c)
{
    static struct Pixmap p;
    static int col;

    if (0 == m_GraphicAssembledBytes)
    {
        m_GraphicsMode = c;
        m_GraphicAssembledBytes = 1;
    }
    else if (m_GraphicAssembledBytes == 1)
    {
        m_GraphicsNrColumns = (unsigned char) c;
        m_GraphicAssembledBytes = 2;
    }
    else if (m_GraphicAssembledBytes == 2)
    {
        int nh = (unsigned char) c;
        m_GraphicsNrColumns += nh * 256;
        m_GraphicAssembledBytes = 3;
        p.map.clear();
    }
    else
    {
        if (m_GraphicAssembledBytes == 3)
        {
            m_GraphicsMaxBytes = m_GraphicsNrColumns * 3;
            std::cout << "Graphics definition mode (" << (unsigned int) m_GraphicsMode;
            std::cout << ") colums (" << m_GraphicsNrColumns;
            std::cout << ") bytes (" << m_GraphicsMaxBytes << ")" << std::endl;
        }

        int rowGroup = m_GraphicAssembledBytes % 3;
        col |= ((unsigned int) c) << (8 * (2 - rowGroup));
        if (2 == rowGroup)
        {
            p.map.push_back(col);
            col = 0;
        }

        ++m_GraphicAssembledBytes;
        if (m_GraphicAssembledBytes >= (m_GraphicsMaxBytes + 3))
        {
            std::cout << std::endl;
            std::cout << "Finished graphics" << std::endl;
            m_InputState = InputState::InputNormal; // Leave escape state
            ctty.append(p);
        }
    }
}
