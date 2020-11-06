/*
 * This file is part of OpenATS COMPASS.
 *
 * COMPASS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * COMPASS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with COMPASS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "createassociationsjob.h"
#include "compass.h"
#include "buffer.h"
#include "createassociationstask.h"
#include "dbinterface.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "metadbovariable.h"
#include "dbovariable.h"
#include "stringconv.h"

#include <ogr_spatialref.h>

#include <tbb/tbb.h>

using namespace std;
using namespace Utils;

bool CreateAssociationsJob::in_appimage_ {getenv("APPDIR")};

CreateAssociationsJob::CreateAssociationsJob(CreateAssociationsTask& task, DBInterface& db_interface,
                                             std::map<std::string, std::shared_ptr<Buffer>> buffers)
    : Job("CreateAssociationsJob"), task_(task), db_interface_(db_interface), buffers_(buffers)
{
}

CreateAssociationsJob::~CreateAssociationsJob() {}

void CreateAssociationsJob::run()
{
    logdbg << "CreateAssociationsJob: run: start";

    started_ = true;

    boost::posix_time::ptime start_time;
    boost::posix_time::ptime stop_time;

    start_time = boost::posix_time::microsec_clock::local_time();

    loginf << "CreateARTASAssociationsJob: run: clearing associations";

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    object_man.removeAssociations();

    // create target reports
    emit statusSignal("Creating Target Reports");
    createTargetReports();

    // create tracker utns
    emit statusSignal("Creating Tracker UTNs");
    createTrackerUTNS();

    unsigned int multiple_associated {0};
    unsigned int single_associated {0};

    for (auto& target_it : targets_)
    {
        if (target_it.second.ds_ids_.size() > 1)
            ++multiple_associated;
        else
            ++single_associated;
    }

    loginf << "CreateARTASAssociationsJob: run: tracker targets " << targets_.size()
           << " multiple " << multiple_associated << " single " << single_associated;

    // create non-tracker utns
    emit statusSignal("Creating non-Tracker UTNS");
    createNonTrackerUTNS();

    // create associations
    emit statusSignal("Creating Associations");
    createAssociations();

    // save associations
    emit statusSignal("Saving Associations");
    for (auto& dbo_it : object_man)
    {
        loginf << "CreateARTASAssociationsJob: run: processing object " << dbo_it.first
               << " associated " << dbo_it.second->associations().size() << " of "
               << dbo_it.second->count();
        dbo_it.second->saveAssociations();
    }

    object_man.setAssociationsByAll(); // no specific dbo or data source

    stop_time = boost::posix_time::microsec_clock::local_time();

    double load_time;
    boost::posix_time::time_duration diff = stop_time - start_time;
    load_time = diff.total_milliseconds() / 1000.0;

    loginf << "CreateARTASAssociationsJob: run: done ("
           << String::doubleToStringPrecision(load_time, 2) << " s).";
    done_ = true;
}

void CreateAssociationsJob::createTargetReports()
{
    loginf << "CreateAssociationsJob: createTargetReports";

    MetaDBOVariable* meta_key_var = task_.keyVar();
    MetaDBOVariable* meta_ds_id_var = task_.dsIdVar();
    MetaDBOVariable* meta_tod_var = task_.todVar();
    MetaDBOVariable* meta_ta_var = task_.targetAddrVar();
    MetaDBOVariable* meta_ti_var = task_.targetIdVar();
    MetaDBOVariable* meta_tn_var = task_.trackNumVar();
    MetaDBOVariable* meta_mode_3a_var = task_.mode3AVar();
    MetaDBOVariable* meta_mode_c_var = task_.modeCVar();
    MetaDBOVariable* meta_latitude_var = task_.latitudeVar();
    MetaDBOVariable* meta_longitude_var = task_.longitudeVar();

    assert (meta_key_var);
    assert (meta_ds_id_var);
    assert (meta_tod_var);
    assert (meta_ta_var);
    assert (meta_ti_var);
    assert (meta_tn_var);
    assert (meta_mode_3a_var);
    assert (meta_mode_c_var);
    assert (meta_latitude_var);
    assert (meta_longitude_var);

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    Association::TargetReport tr;

    for (auto& buf_it : buffers_) // dbo name, buffer
    {
        string dbo_name = buf_it.first;
        DBObject& dbo = object_man.object(dbo_name);

        shared_ptr<Buffer> buffer = buf_it.second;
        size_t buffer_size = buffer->size();

        assert (meta_key_var->existsIn(dbo_name));
        DBOVariable& key_var = meta_key_var->getFor(dbo_name);

        assert (meta_ds_id_var->existsIn(dbo_name));
        DBOVariable& ds_id_var = meta_ds_id_var->getFor(dbo_name);

        assert (meta_tod_var->existsIn(dbo_name));
        DBOVariable& tod_var = meta_tod_var->getFor(dbo_name);

        assert (meta_ta_var->existsIn(dbo_name));
        DBOVariable& ta_var = meta_ta_var->getFor(dbo_name);

        assert (meta_ti_var->existsIn(dbo_name));
        DBOVariable& ti_var = meta_ti_var->getFor(dbo_name);

        DBOVariable* tn_var {nullptr}; // not in ads-b
        if (meta_tn_var->existsIn(dbo_name))
            tn_var = &meta_tn_var->getFor(dbo_name);

        assert (meta_mode_3a_var->existsIn(dbo_name));
        DBOVariable& mode_3a_var = meta_mode_3a_var->getFor(dbo_name);

        assert (meta_mode_c_var->existsIn(dbo_name));
        DBOVariable& mode_c_var = meta_mode_c_var->getFor(dbo_name);

        assert (meta_latitude_var->existsIn(dbo_name));
        DBOVariable& latitude_var = meta_latitude_var->getFor(dbo_name);

        assert (meta_longitude_var->existsIn(dbo_name));
        DBOVariable& longitude_var = meta_longitude_var->getFor(dbo_name);


        assert (buffer->has<int>(key_var.name()));
        NullableVector<int>& rec_nums = buffer->get<int>(key_var.name());

        assert (buffer->has<int>(ds_id_var.name()));
        NullableVector<int>& ds_ids = buffer->get<int>(ds_id_var.name());

        assert (buffer->has<float>(tod_var.name()));
        NullableVector<float>& tods = buffer->get<float>(tod_var.name());

        assert (buffer->has<int>(ta_var.name()));
        NullableVector<int>& tas = buffer->get<int>(ta_var.name());

        assert (buffer->has<string>(ti_var.name()));
        NullableVector<string>& tis = buffer->get<string>(ti_var.name());

        NullableVector<int>* tns {nullptr};
        if (tn_var)
        {
            assert (buffer->has<int>(tn_var->name()));
            tns = &buffer->get<int>(tn_var->name());
        }

        assert (buffer->has<int>(mode_3a_var.name()));
        NullableVector<int>& m3as = buffer->get<int>(mode_3a_var.name());

        assert (buffer->has<int>(mode_c_var.name()));
        NullableVector<int>& mcs = buffer->get<int>(mode_c_var.name());

        assert (buffer->has<double>(latitude_var.name()));
        NullableVector<double>& lats = buffer->get<double>(latitude_var.name());

        assert (buffer->has<double>(longitude_var.name()));
        NullableVector<double>& longs = buffer->get<double>(longitude_var.name());

        for (size_t cnt = 0; cnt < buffer_size; ++cnt)
        {
            assert (!rec_nums.isNull(cnt));
            assert (!ds_ids.isNull(cnt));

            tr.dbo_name_ = dbo_name;
            tr.rec_num_ = rec_nums.get(cnt);
            tr.ds_id_ = ds_ids.get(cnt);

            if (tods.isNull(cnt))
            {
                logwrn << "CreateAssociationsJob: createTargetReports: target report w/o time: dbo "
                       << dbo_name << " rec_num " << tr.rec_num_  << " ds_id " << tr.ds_id_;
                continue;
            }

            if (tods.isNull(cnt))
            {
                logwrn << "CreateAssociationsJob: createTargetReports: target report w/o time: dbo "
                       << dbo_name << " rec_num " << tr.rec_num_  << " ds_id " << tr.ds_id_;
                continue;
            }

            if (lats.isNull(cnt))
            {
                logwrn << "CreateAssociationsJob: createTargetReports: target report w/o latitude: dbo "
                       << dbo_name << " rec_num " << tr.rec_num_  << " ds_id " << tr.ds_id_;
                continue;
            }
            if (longs.isNull(cnt))
            {
                logwrn << "CreateAssociationsJob: createTargetReports: target report w/o longitude: dbo "
                       << dbo_name << " rec_num " << tr.rec_num_  << " ds_id " << tr.ds_id_;
                continue;
            }

            tr.tod_ = tods.get(cnt);

            tr.has_ta_ = !tas.isNull(cnt);
            tr.ta_ = tr.has_ta_ ? tas.get(cnt) : 0;

            tr.has_ti_ = !tis.isNull(cnt);
            tr.ti_ = tr.has_ti_ ? tis.get(cnt) : "";

            tr.has_tn_ = tns && !tns->isNull(cnt);
            tr.tn_ = tr.has_tn_ ? tns->get(cnt) : 0;

            tr.has_ma_ = !m3as.isNull(cnt);
            tr.ma_ = tr.has_ma_ ? m3as.get(cnt) : 0;

            tr.has_ma_v_ = false; // TODO
            tr.has_ma_g_ = false; // TODO

            tr.has_mc_ = !mcs.isNull(cnt);
            tr.mc_ = tr.has_mc_ ? mcs.get(cnt) : 0;

            tr.has_mc_v_ = false; // TODO

            tr.latitude_ = lats.get(cnt);
            tr.longitude_ = longs.get(cnt);

            target_reports_[dbo_name][tr.ds_id_].push_back(tr);
        }
    }
}

void CreateAssociationsJob::createTrackerUTNS()
{
    loginf << "CreateAssociationsJob: createTrackerUTNS";

    if (target_reports_.count("Tracker"))
    {
        std::map<unsigned int, std::vector<Association::TargetReport>>& ds_id_trs = target_reports_.at("Tracker");

        unsigned int utn;

        map<unsigned int, Association::Target> tracker_targets;
        map<unsigned int, pair<unsigned int, float>> tn2utn; // track num -> utn, last tod

        DBObjectManager& object_man = COMPASS::instance().objectManager();

        // create utn for all tracks
        for (auto& ds_it : ds_id_trs) // ds_id->trs
        {
            loginf << "CreateAssociationsJob: createTrackerUTNS: processing ds_id " << ds_it.first;

            tracker_targets.clear();
            tn2utn.clear();

            string ds_name = object_man.object("Tracker").dataSources().at(ds_it.first).name();

            unsigned int tmp_utn_cnt {0};

            for (auto& tr_it : ds_it.second)
            {
                if (tr_it.has_tn_)
                {
                    if (!tn2utn.count(tr_it.tn_)) // first track update exists
                    {
                        logdbg << "CreateAssociationsJob: createTrackerUTNS: creating new tmp utn " << tmp_utn_cnt
                               << " for tn " << tr_it.tn_;

                        tn2utn[tr_it.tn_] = {tmp_utn_cnt, tr_it.tod_};
                        ++tmp_utn_cnt;
                    }

                    if (tn2utn.at(tr_it.tn_).second > tr_it.tod_)
                    {
                        logwrn << "CreateAssociationsJob: createTrackerUTNS: tod backjump -"
                               << String::timeStringFromDouble(tn2utn.at(tr_it.tn_).second-tr_it.tod_)
                               << " tmp utn " << tmp_utn_cnt << " at tr " << tr_it.asStr();
                    }
                    assert (tn2utn.at(tr_it.tn_).second <= tr_it.tod_);

                    if (tr_it.tod_ - tn2utn.at(tr_it.tn_).second > 60.0) // gap, new track
                    {
                        logdbg << "CreateAssociationsJob: createTrackerUTNS: creating new tmp utn " << tmp_utn_cnt
                               << " for tn " << tr_it.tn_ << " because of gap "
                               << String::timeStringFromDouble(tr_it.tod_ - tn2utn.at(tr_it.tn_).second);

                        tn2utn[tr_it.tn_] = {tmp_utn_cnt, tr_it.tod_};
                        ++tmp_utn_cnt;
                    }

                    assert (tn2utn.count(tr_it.tn_));
                    utn = tn2utn.at(tr_it.tn_).first;
                    tn2utn.at(tr_it.tn_).second = tr_it.tod_;

                    if (!tracker_targets.count(utn)) // add new target if not existing
                        tracker_targets.emplace(
                                    std::piecewise_construct,
                                    std::forward_as_tuple(utn),   // args for key
                                    std::forward_as_tuple(utn, true));  // args for mapped value

                    tracker_targets.at(utn).addAssociated(&tr_it);
                }
                else
                {
                    logwrn << "CreateAssociationsJob: createTrackerUTNS: tracker target report w/o track num in ds_id "
                           << tr_it.ds_id_ << " at tod " << String::timeStringFromDouble(tr_it.tod_);
                }
            }

            if (!tracker_targets.size())
            {
                logwrn << "CreateAssociationsJob: createTrackerUTNS: tracker ds_id " << ds_it.first
                       << " created no utns";
                continue;
            }

            loginf << "CreateAssociationsJob: createTrackerUTNS: creating utns for ds_id " << ds_it.first;

            // tracker_targets exist, tie them together by mode s address

            int tmp_utn;

            float done_ratio;
            unsigned int targets_size = tracker_targets.size();
            unsigned int target_cnt = 0;

            while (tracker_targets.size())
            {
                done_ratio = (float)target_cnt / (float)targets_size;
                emit statusSignal(("Creating Tracker UTNs DS "
                                   +ds_name+" ("+String::percentToString(100.0*done_ratio)+"%)").c_str());

                ++target_cnt;

                auto tmp_target = tracker_targets.begin();
                assert (tmp_target != tracker_targets.end());

                logdbg << "CreateAssociationsJob: createTrackerUTNS: creating utn for tmp utn " << tmp_target->first;

                tmp_utn = findUTNForTarget(tmp_target->second);

                if (tmp_utn == -1) // none found, create new target
                    addTarget(tmp_target->second);
                else // attach to existing target
                {
                    assert (targets_.count(tmp_utn));
                    targets_.at(tmp_utn).addAssociated(tmp_target->second.assoc_trs_);
                }

                tracker_targets.erase(tmp_target);
            }

            loginf << "CreateAssociationsJob: createTrackerUTNS: processing ds_id " << ds_it.first << " done";
        }
    }
    else
        loginf << "CreateAssociationsJob: createTrackerUTNS: no tracker data";

}

void CreateAssociationsJob::createNonTrackerUTNS()
{
    loginf << "CreateAssociationsJob: createNonTrackerUTNS";

    int tmp_utn;

    for (auto& dbo_it : target_reports_)
    {
        if (dbo_it.first == "Tracker") // already associated
            continue;

        for (auto& ds_it : dbo_it.second) // ds_id -> trs
        {
            for (auto& tr_it : ds_it.second)
            {
                tmp_utn = findUTNForTargetReport(tr_it);

                if (tmp_utn != -1) // existing target found
                {
                    assert (targets_.count(tmp_utn));
                    targets_.at(tmp_utn).addAssociated(&tr_it);
                }
                else if (tr_it.has_ta_)
                {
                    addTargetByTargetReport(tr_it);
                }
                else
                    ; // nothing can be done at the momemt
            }
        }
    }
}

void CreateAssociationsJob::createAssociations()
{
    loginf << "CreateAssociationsJob: createAssociations";

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    for (auto& dbo_it : target_reports_)
    {
        assert (object_man.existsObject(dbo_it.first));
        DBObject& dbo = object_man.object(dbo_it.first);

        for (auto& ds_it : dbo_it.second) // ds_id -> trs
        {
            for (auto& tr_it : ds_it.second)
            {
                for (auto utn_ptr_it : tr_it.assoc_targets_)
                    dbo.addAssociation(tr_it.rec_num_, utn_ptr_it->utn_, false, 0);
            }
        }
    }
}

int CreateAssociationsJob::findUTNForTarget (const Association::Target& target)
// tries to find existing utn for target, -1 if failed
{
    if (target.has_ta_ && ta_2_utn_.count(target.ta_))
        return ta_2_utn_.at(target.ta_);
    else
    {
        logdbg << "CreateAssociationsJob: findUTNForTarget: checking target " << target.utn_;

        OGRSpatialReference wgs84;
        wgs84.SetWellKnownGeogCS("WGS84");

        //for (auto& other_it : targets_)
        tbb::parallel_for(uint(0), utn_cnt_, [&](unsigned int cnt)
        {
            Association::Target& other = targets_.at(cnt);

            logdbg << "CreateAssociationsJob: findUTNForTarget: checking target " << target.utn_
                   << " other " << other.utn_;

            if (target.timeOverlaps(other))
            {
                vector<float> unknown;
                vector<float> same;
                vector<float> different;

                tie (unknown, same, different) = target.compareModeACodes(other);

                logdbg << "CreateAssociationsJob: findUTNForTarget: unknown "
                       << unknown.size() << " same " << same.size() << " different " << different.size();

                if (same.size() > different.size())
                {
                    vector<pair<float, double>> same_distances;
                    double distances_sum {0};

                    OGRSpatialReference local;

                    std::unique_ptr<OGRCoordinateTransformation> ogr_geo2cart;

                    EvaluationTargetPosition tst_pos;

                    double x_pos, y_pos;
                    double distance;

                    EvaluationTargetPosition ref_pos;
                    bool ok;

                    for (auto tod_it : same)
                    {
                        assert (target.hasDataForExactTime(tod_it));
                        tst_pos = target.posForExactTime(tod_it);

                        tie(ref_pos, ok) = other.interpolatedPosForTime(tod_it, 15.0);

                        if (!ok)
                            continue;

                        local.SetStereographic(ref_pos.latitude_, ref_pos.longitude_, 1.0, 0.0, 0.0);

                        ogr_geo2cart.reset(OGRCreateCoordinateTransformation(&wgs84, &local));

                        if (in_appimage_) // inside appimage
                        {
                            x_pos = tst_pos.longitude_;
                            y_pos = tst_pos.latitude_;
                        }
                        else
                        {
                            x_pos = tst_pos.latitude_;
                            y_pos = tst_pos.longitude_;
                        }

                        ok = ogr_geo2cart->Transform(1, &x_pos, &y_pos); // wgs84 to cartesian offsets

                        if (!ok)
                            continue;

                        distance = sqrt(pow(x_pos,2)+pow(y_pos,2));

                        if (distance > 50000)
                        {
                            same_distances.clear();
                            break;
                        }

                        //loginf << "\tdist " << distance;

                        same_distances.push_back({tod_it, distance});
                        distances_sum += distance;
                    }

                    if (same_distances.size())
                    {
                        double distance_avg = distances_sum / (float) same_distances.size();

                        if (distance_avg < 10000)
                            loginf << "\ttarget " << target.utn_ << " other " << other.utn_
                                   << " dist avg " << distance_avg << " num " << same_distances.size();
                    }
                }
            }
            //            else
            //                loginf << "\tno overlap";
        });

        return -1;
    }
}

int CreateAssociationsJob::findUTNForTargetReport (const Association::TargetReport& tr)
{
    if (tr.has_ta_ && ta_2_utn_.count(tr.ta_))
        return ta_2_utn_.at(tr.ta_);
    else
        return -1;
}


void CreateAssociationsJob::addTarget (const Association::Target& target) // creates new utn, adds to targets_
{
    if (target.has_ta_)
    {
        assert (!ta_2_utn_.count(target.ta_));
        ta_2_utn_[target.ta_] = utn_cnt_;
    }

    //targets_[utn_cnt_] = {utn_cnt_};

    targets_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(utn_cnt_),   // args for key
                std::forward_as_tuple(utn_cnt_, false));  // args for mapped value

    targets_.at(utn_cnt_).addAssociated(target.assoc_trs_);

    ++utn_cnt_;
}

void CreateAssociationsJob::addTargetByTargetReport (Association::TargetReport& tr)
{
    if (tr.has_ta_)
    {
        assert (!ta_2_utn_.count(tr.ta_));
        ta_2_utn_[tr.ta_] = utn_cnt_;
    }

    //targets_[utn_cnt_] = {utn_cnt_};
    targets_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(utn_cnt_),   // args for key
                std::forward_as_tuple(utn_cnt_, false));  // args for mapped value

    targets_.at(utn_cnt_).addAssociated(&tr);

    ++utn_cnt_;
}
