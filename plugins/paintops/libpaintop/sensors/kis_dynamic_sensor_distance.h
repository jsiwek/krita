/*
 *  Copyright (c) 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _KIS_DYNAMIC_SENSOR_DISTANCE_H_
#define _KIS_DYNAMIC_SENSOR_DISTANCE_H_

#include "kis_dynamic_sensor.h"
//
class KisDynamicSensorDistance : public QObject, public KisDynamicSensor
{
    Q_OBJECT
public:
    using KisSerializableConfiguration::fromXML;
    using KisSerializableConfiguration::toXML;


    KisDynamicSensorDistance();
    ~KisDynamicSensorDistance() override { }
    qreal value(const KisPaintInformation&) override;
    void reset() override;
    QWidget* createConfigurationWidget(QWidget* parent, QWidget*) override;
public Q_SLOTS:
    virtual void setPeriodic(bool periodic);
    virtual void setLength(int length);

    void toXML(QDomDocument&, QDomElement&) const override;
    void fromXML(const QDomElement&) override;
private:
    qreal m_measuredDistance;
    bool m_periodic;
};

#endif
