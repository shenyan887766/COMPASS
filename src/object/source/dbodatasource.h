/*
 * This file is part of ATSDB.
 *
 * ATSDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ATSDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with ATSDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DBODATASOURCE_H
#define DBODATASOURCE_H

#include "dboeditdatasourceactionoptions.h"

#include "json.hpp"

#include <memory>

class DBObject;

class DBODataSource
{
  public:
    DBODataSource(DBObject& object, unsigned int id, const std::string& name);
    DBODataSource() = default;

    // copy from dbds, everything but id
    DBODataSource& operator=(StoredDBODataSource& other);
    // DBODataSource& operator=(DBODataSource&& other);

    // comparison
    bool operator==(const StoredDBODataSource& other) const;
    bool operator!=(const StoredDBODataSource& other) const { return !(*this == other); }

    virtual ~DBODataSource();

    const std::string dboName() const;

    unsigned int id() const;
    // void id(unsigned int id);

    const std::string& name() const;
    void name(const std::string& name);

    bool hasShortName() const;
    void removeShortName();
    void shortName(const std::string& short_name);
    const std::string& shortName() const;

    bool hasSac() const;
    void removeSac();
    void sac(unsigned char sac);
    unsigned char sac() const;

    bool hasSic() const;
    void removeSic();
    void sic(unsigned char sic);
    unsigned char sic() const;

    bool hasLatitude() const;
    void removeLatitude();
    void latitude(double latitiude);
    double latitude() const;

    bool hasLongitude() const;
    void removeLongitude();
    void longitude(double longitude_);
    double longitude() const;

    bool hasAltitude() const;
    void removeAltitude();
    void altitude(double altitude);
    double altitude() const;

    // psr
    bool hasPrimaryAzimuthStdDev() const;
    void removePrimaryAzimuthStdDev();
    void primaryAzimuthStdDev(double value);
    double primaryAzimuthStdDev() const;

    bool hasPrimaryRangeStdDev() const;
    void removePrimaryRangeStdDev();
    void primaryRangeStdDev(double value);
    double primaryRangeStdDev() const;

    //ssr
    bool hasSecondaryAzimuthStdDev() const;
    void removeSecondaryAzimuthStdDev();
    void secondaryAzimuthStdDev(double value);
    double secondaryAzimuthStdDev() const;

    bool hasSecondaryRangeStdDev() const;
    void removeSecondaryRangeStdDev();
    void secondaryRangeStdDev(double value);
    double secondaryRangeStdDev() const;

    // mode s
    bool hasModeSAzimuthStdDev() const;
    void removeModeSAzimuthStdDev();
    void modeSAzimuthStdDev(double value);
    double modeSAzimuthStdDev() const;

    bool hasModeSRangeStdDev() const;
    void removeModeSRangeStdDev();
    void modeSRangeStdDev(double value);
    double modeSRangeStdDev() const;

    DBObject& object();
    void updateInDatabase();  // not called automatically in setters

    void print() const;

//    nlohmann::json dbContent() const;
//    void dbContent(const nlohmann::json& dbContent);

protected:
    DBObject* object_;
    unsigned int id_{0};

    std::string name_;

    bool has_short_name_{false};
    std::string short_name_;

    bool has_sac_{false};
    unsigned char sac_{0};

    bool has_sic_{false};
    unsigned char sic_{0};

    bool has_latitude_{false};
    double latitude_{0};  // degrees

    bool has_longitude_{false};
    double longitude_{0};  // degrees

    bool has_altitude_{false};
    double altitude_{0};  // meter above msl

    // radar specific stuff

    // pri azm bias
    // pri range bias
    // pri range gain

    // pri azm standard dev
    bool has_primary_azimuth_stddev_{false};
    double primary_azimuth_stddev_ {0};  // degrees
    // pri range standard dev
    bool has_primary_range_stddev_{false};
    double primary_range_stddev_ {0};  // meters

    // ssr azm bias
    // ssr range bias
    // ssr range gain

    // ssr azm standard dev
    bool has_secondary_azimuth_stddev_{false};
    double secondary_azimuth_stddev_ {0};  // degrees
    // ssr range standard dev
    bool has_secondary_range_stddev_{false};
    double secondary_range_stddev_ {0};  // meters

    // mode s azm bias
    // mode s range bias
    // mode s range gain

    // mode s azm standard dev
    bool has_mode_s_azimuth_stddev_{false};
    double mode_s_azimuth_stddev_ {0};  // degrees
    // mode s range standard dev
    bool has_mode_s_range_stddev_{false};
    double mode_s_range_stddev_ {0};  // meters

//    nlohmann::json db_content_;
};

#endif  // DBODATASOURCE_H
