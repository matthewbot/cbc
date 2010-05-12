/**************************************************************************
 *  Copyright 2008,2009 KISS Institute for Practical Robotics             *
 *                                                                        *
 *  This file is part of CBC Firmware.                                    *
 *                                                                        *
 *  CBC Firmware is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  CBC Firmware is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this copy of CBC Firmware.  Check the LICENSE file         *
 *  in the project root.  If not, see <http://www.gnu.org/licenses/>.     *
 **************************************************************************/

#ifndef __PAGE_H__
#define __PAGE_H__

#include <QWidget>
#include <QList>

class Page : public QWidget
{
    Q_OBJECT

public:
    Page(QWidget *parent = 0, bool nodim=false);
    ~Page();
    
    inline bool getNoDim() { return m_nodim; }
    
    static Page *currentPage();
    Page *lastPage();
    
    virtual void show();
    virtual void hide();

public slots:
   void raiseLastPage();
   void raisePage();

private:
   static Page *m_currentPage;
   Page *m_lastPage;
   bool m_nodim;
};

#endif
