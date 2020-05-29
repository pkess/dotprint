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

#include <iostream>
#include <assert.h>
#include "CairoTTY.h"
#include "CodepageTranslator.h"

CairoTTY::CairoTTY(Cairo::RefPtr<Cairo::PdfSurface> cs, const PageSize &p, const Margins &m, ICharPreprocessor *preprocessor):
    m_CairoSurface(cs),
    m_Margins(m),
    m_Preprocessor(preprocessor),
    m_tabWidth(8)
{
    m_Context = Cairo::Context::create(m_CairoSurface);
    m_CpTranslator = new CodepageTranslator();

    SetPageSize(p);
    StretchFont(1.0, 1.0);
    SetFont("Courier New", 10.0);
    m_lineSpacing = m_FontExtents.height * m_StretchY;
    Home();
}

CairoTTY::~CairoTTY()
{
    m_Context.clear();
    m_CairoSurface->finish();
}

CairoTTY &CairoTTY::operator<<(const Glib::ustring &s)
{
    for (gunichar c : s)
        operator<<(c);

    return *this;
}

CairoTTY &CairoTTY::operator<<(gunichar c)
{
    if (m_Preprocessor)
        m_Preprocessor->process(*this, c);
    else
        append(c);

    return *this;
}

void CairoTTY::SetPreprocessor(ICharPreprocessor *preprocessor)
{
    m_Preprocessor = preprocessor;
}

void CairoTTY::SetFont(const std::string &family, double size, Cairo::FontSlant slant, Cairo::FontWeight weight)
{
    assert(size > 0.0);

    m_Context->select_font_face(family, slant, weight);
    m_Context->set_font_size(size);

    m_Context->get_font_extents(m_FontExtents);
}

void CairoTTY::SetPageSize(const PageSize &p)
{
    assert(p.m_Width > 0.0);
    assert(p.m_Height > 0.0);

    m_PageSize = p;
    m_CairoSurface->set_size(m_PageSize.m_Width, m_PageSize.m_Height);
}

void CairoTTY::Home()
{
    m_x = 0.0;
    m_y = m_lineSpacing; // so that the top of the first line touches 0.0
}

void CairoTTY::NewLine()
{
    CarriageReturn();
    LineFeed();
}

void CairoTTY::CarriageReturn()
{
    m_x = 0.0;
}

void CairoTTY::LineFeed()
{
    m_y += m_lineSpacing;

    // check if we still fit on the page
    if (m_Margins.m_Top + m_y > m_PageSize.m_Height - m_Margins.m_Bottom)
        NewPage(); // forced pagebreak
}

void CairoTTY::NewPage()
{
    m_Context->show_page();
    Home();
}

void CairoTTY::SetLineSpacing(double spacing)
{
    m_lineSpacing = 69.0 *  spacing;
}

void CairoTTY::SetTabWidth(int spaces)
{

    m_tabWidth = spaces;
}

void CairoTTY::StretchFont(double stretch_x, double stretch_y)
{
    m_StretchX = stretch_x;
    m_StretchY = stretch_y;
}

void CairoTTY::append(gunichar c)
{
    gunichar uc;

    if (m_CpTranslator->translate(c, uc))
    {
        if (c == 0x09)
        {
            // TODO: tab handling
            for (int i = 0; i < m_tabWidth; ++i)
            {
                append((gunichar) ' ');
            }
            return;
        }
        Glib::ustring s(1, uc);

        Cairo::TextExtents t;
        m_Context->get_text_extents(s, t);
        double x_advance = m_StretchX * t.x_advance;

        if (m_Margins.m_Left + m_x + x_advance > m_PageSize.m_Width - m_Margins.m_Right)
            NewLine(); // forced linebreak - text wraps to the next line

        m_Context->save();
        m_Context->move_to(m_Margins.m_Left + m_x, m_Margins.m_Top + m_y);
        m_Context->scale(m_StretchX, m_StretchY);
        m_Context->show_text(s);
        m_Context->restore();

        // We ignore y_advance, as we in no way can support
        // vertical text layout.
        m_x += x_advance;
    }
}

void CairoTTY::append(Pixmap p)
{
    double x0 = m_Margins.m_Left + m_x -4.0;
    double y0 = m_Margins.m_Top + m_y -6.0;
    double x = 0;
    double y = 0;
    double x_inc = 0.55;
    double y_inc = 0.36;

    m_Context->save();
    m_Context->set_line_width(0.4);
    for (auto &row : p.map)
    {
        y = 0;
        double x_off = x0 + (x * x_inc);
        for (int i = 0; i < 24; ++i)
        {
            double y_off = y0 + (y * y_inc);
            if (row & (1 << (23 - i)))
            {
                m_Context->move_to(x_off, y_off);
                m_Context->line_to(x_off + 0.6, y_off);
                m_Context->line_to(x_off + 0.6, y_off + y_inc);
                m_Context->line_to(x_off, y_off + y_inc);
            }
            ++y;
        }
        ++x;
    }
    m_Context->stroke();
    m_Context->restore();
}
