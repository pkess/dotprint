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
#include <stdexcept>

#include <assert.h>

#include <unistd.h>
#include <getopt.h>

#include "CairoTTY.h"
#include "PageSizeFactory.h"
#include "CmdLineParser.h"

class UniFileStream
{
public:
    UniFileStream(const std::string &fname):
        m_io(nullptr),
        m_eof(false)
    {
        GError *err = nullptr;

        m_io = g_io_channel_new_file(fname.c_str(), "r", &err);
        if (!m_io)
        {
            assert(err != nullptr);

            throw std::runtime_error(std::string("Cannot open input file: ") + err->message);
        }
    }

    bool ReadUniChar(gunichar &c)
    {
        assert(m_io != nullptr);
        GIOStatus status = g_io_channel_read_unichar(m_io, &c, nullptr);

        if (status == G_IO_STATUS_NORMAL)
            return true;

        if (status == G_IO_STATUS_EOF)
            m_eof = true;
        else
            throw std::runtime_error("can't read fron input file!");

        c = 0;
        return false;
    }

    bool Eof() const
    {
        return m_eof;
    }

    ~UniFileStream()
    {
        assert(m_io != nullptr);
        g_io_channel_shutdown (m_io, TRUE, nullptr);
    }

protected:
    GIOChannel *m_io;
    bool m_eof;
};

int main(int argc, char *argv[])
{
    CmdLineParser cmdline(argc, argv);

    PageSize p = cmdline.GetPageSize();
    if (cmdline.GetLandscape())
        p.Landscape();

    ICharPreprocessor *preproc = cmdline.GetPreprocessor();

    Cairo::RefPtr<Cairo::PdfSurface> cs = Cairo::PdfSurface::create(cmdline.GetOutputFile(), p.m_Width, p.m_Height);
    assert(cs);

    Margins m(10.0 * milimeter,10.0 * milimeter,10.0 * milimeter);

    CairoTTY ctty(cs, p, m, preproc);

    // copy the file into ctty
    ctty.SetFont(cmdline.GetFontFace(), cmdline.GetFontSize(), Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);

    UniFileStream f(cmdline.GetInputFile());

    gunichar c;
    while (f.ReadUniChar(c))
    {
        ctty << c;
    }

    return 0;
}
