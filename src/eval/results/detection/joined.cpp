#include "eval/results/detection/joined.h"
#include "eval/results/detection/single.h"
#include "eval/requirement/base.h"
#include "eval/requirement/detection/detection.h"
#include "evaluationtargetdata.h"
#include "evaluationmanager.h"
#include "eval/results/report/rootitem.h"
#include "eval/results/report/section.h"
#include "eval/results/report/sectioncontenttext.h"
#include "eval/results/report/sectioncontenttable.h"
#include "logger.h"
#include "stringconv.h"

#include <cassert>

using namespace std;
using namespace Utils;

namespace EvaluationRequirementResult
{

    JoinedDetection::JoinedDetection(
            const std::string& result_id, std::shared_ptr<EvaluationRequirement::Base> requirement,
            const SectorLayer& sector_layer, EvaluationManager& eval_man)
        : Joined("JoinedDetection", result_id, requirement, sector_layer, eval_man)
    {
    }


    void JoinedDetection::join(std::shared_ptr<Base> other)
    {
        Joined::join(other);

        std::shared_ptr<SingleDetection> other_sub =
                std::static_pointer_cast<SingleDetection>(other);
        assert (other_sub);

        addToValues(other_sub);
    }

    void JoinedDetection::addToValues (std::shared_ptr<SingleDetection> single_result)
    {
        assert (single_result);

        if (!single_result->use())
            return;

        sum_uis_ += single_result->sumUIs();
        missed_uis_ += single_result->missedUIs();

        updatePD();
    }

    void JoinedDetection::updatePD()
    {
        if (sum_uis_)
        {
            logdbg << "JoinedDetection: updatePD: result_id " << result_id_ << " missed_uis " << missed_uis_
                   << " sum_uis " << sum_uis_;

            assert (missed_uis_ <= sum_uis_);

            pd_ = 1.0 - ((float)missed_uis_/(float)(sum_uis_));
            has_pd_ = true;
        }
        else
        {
            pd_ = 0;
            has_pd_ = false;
        }
    }

    void JoinedDetection::print()
    {
        std::shared_ptr<EvaluationRequirement::Detection> req =
                std::static_pointer_cast<EvaluationRequirement::Detection>(requirement_);
        assert (req);

        if (sum_uis_)
            loginf << "JoinedDetection: print: req. name " << req->name()
                   << " pd " << String::percentToString(100.0 * pd_) << " passed " << (pd_ >= req->minimumProbability());
        else
            loginf << "JoinedDetection: print: req. name " << req->name()
                   << " has no data";
    }

    void JoinedDetection::addToReport (
            std::shared_ptr<EvaluationResultsReport::RootItem> root_item)
    {
        logdbg << "JoinedDetection " <<  requirement_->name() <<": addToReport";

        if (!results_.size()) // some data must exist
        {
            logerr << "JoinedDetection " <<  requirement_->name() <<": addToReport: no data";
            return;
        }

        logdbg << "JoinedDetection " <<  requirement_->name() << ": addToReport: adding joined result";

        addToOverviewTable(root_item);
        addDetails(root_item);
    }

    void JoinedDetection::addToOverviewTable(std::shared_ptr<EvaluationResultsReport::RootItem> root_item)
    {
        EvaluationResultsReport::SectionContentTable& ov_table = getReqOverviewTable(root_item);

        // condition
        std::shared_ptr<EvaluationRequirement::Detection> req =
                std::static_pointer_cast<EvaluationRequirement::Detection>(requirement_);
        assert (req);

        string condition = ">= "+String::percentToString(req->minimumProbability() * 100.0);

        // pd
        QVariant pd_var;

        string result {"Unknown"};

        if (has_pd_)
        {
            pd_var = String::percentToString(pd_ * 100.0).c_str();

            result = pd_ >= req->minimumProbability() ? "Passed" : "Failed";
        }

        // "Req.", "Group", "Result", "Condition", "Result"
        ov_table.addRow({sector_layer_.name().c_str(), requirement_->shortname().c_str(),
                         requirement_->groupName().c_str(), {sum_uis_},
                         pd_var, condition.c_str(), result.c_str()}, this, {});
        // "Report:Results:Overview"
    }

    void JoinedDetection::addDetails(std::shared_ptr<EvaluationResultsReport::RootItem> root_item)
    {
        EvaluationResultsReport::Section& sector_section = getRequirementSection(root_item);

        if (!sector_section.hasTable("sector_details_table"))
            sector_section.addTable("sector_details_table", 3, {"Name", "comment", "Value"}, false);

        EvaluationResultsReport::SectionContentTable& sec_det_table =
                sector_section.getTable("sector_details_table");

        addCommonDetails(sec_det_table);

        sec_det_table.addRow({"#Updates/EUIs [1]", "Total number update intervals", sum_uis_}, this);
        sec_det_table.addRow({"MUIs [1]", "Number of missed update intervals", missed_uis_}, this);

        // condition
        std::shared_ptr<EvaluationRequirement::Detection> req =
                std::static_pointer_cast<EvaluationRequirement::Detection>(requirement_);
        assert (req);

        string condition = ">= "+String::percentToString(req->minimumProbability() * 100.0);

        // pd
        QVariant pd_var;

        string result {"Unknown"};

        if (has_pd_)
        {
            pd_var = String::percentToString(pd_ * 100.0).c_str();

            result = pd_ >= req->minimumProbability() ? "Passed" : "Failed";
        }

        sec_det_table.addRow({"PD [%]", "Probability of Detection", pd_var}, this);
        sec_det_table.addRow({"Condition", {}, condition.c_str()}, this);
        sec_det_table.addRow({"Condition Fulfilled", {}, result.c_str()}, this);

        // figure
        if (has_pd_ && pd_ != 1.0)
        {
            sector_section.addFigure("sector_errors_overview", "Sector Errors Overview",
                                     eval_man_.getViewableForEvaluation(req_grp_id_, result_id_));
        }
        else
        {
            sector_section.addText("sector_errors_overview_no_figure");
            sector_section.getText("sector_errors_overview_no_figure").addText(
                        "No target errors found, therefore no figure was generated.");
        }
    }

    bool JoinedDetection::hasViewableData (
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation)
    {
        if (table.name() == "req_overview")
            return true;
        else
            return false;;
    }

    std::unique_ptr<nlohmann::json::object_t> JoinedDetection::viewableData(
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation)
    {
        assert (hasViewableData(table, annotation));
        return eval_man_.getViewableForEvaluation(req_grp_id_, result_id_);
    }

    bool JoinedDetection::hasReference (
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation)
    {
        if (table.name() == "req_overview")
            return true;
        else
            return false;;
    }

    std::string JoinedDetection::reference(
            const EvaluationResultsReport::SectionContentTable& table, const QVariant& annotation)
    {
        assert (hasReference(table, annotation));
        return "Report:Results:"+getRequirementSectionID();
    }

    void JoinedDetection::updatesToUseChanges()
    {
        loginf << "JoinedDetection: updatesToUseChanges: prev sum_uis " << sum_uis_
               << " missed_uis " << missed_uis_;

        if (has_pd_)
            loginf << "JoinedDetection: updatesToUseChanges: prev result " << result_id_
                   << " pd " << 100.0 * pd_;
        else
            loginf << "JoinedDetection: updatesToUseChanges: prev result " << result_id_ << " has no data";

        sum_uis_ = 0;
        missed_uis_ = 0;

        for (auto result_it : results_)
        {
            std::shared_ptr<SingleDetection> result =
                    std::static_pointer_cast<SingleDetection>(result_it);
            assert (result);

            addToValues(result);
        }

        loginf << "JoinedDetection: updatesToUseChanges: updt sum_uis " << sum_uis_
               << " missed_uis " << missed_uis_;

        if (has_pd_)
            loginf << "JoinedDetection: updatesToUseChanges: updt result " << result_id_
                   << " pd " << 100.0 * pd_;
        else
            loginf << "JoinedDetection: updatesToUseChanges: updt result " << result_id_ << " has no data";
    }

}
