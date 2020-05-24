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
#include <glibmm.h>
#include "SimplePreprocessor.h"

void SimplePreprocessor::process(ICairoTTYProtected &ctty, gunichar c)
{
    if (Glib::Unicode::iscntrl(c))
    {
        // Control codes handled here
        switch (c)
        {
        case '\n':
            ctty.NewLine();
            break;

        case 0x0c:
            ctty.NewPage();
            break;

        default:
            std::cerr << "SimplePreprocessor::process(): ignoring unknown character 0x" << std::hex << c << std::endl;
            //assert(0);
        }
    }
    else
        ctty.append(c);
}