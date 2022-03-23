#include <alex_path_planner_common/DubinsPlan.h>
#include <sstream>

void DubinsPlan::append(const DubinsPlan &plan) {
    for (auto s : plan.m_DubinsPaths) append(s);
}

void DubinsPlan::append(const DubinsWrapper& dubinsPath) {
    m_DubinsPaths.push_back(dubinsPath);
}

void DubinsPlan::sample(State& s) const {
    for (const auto& p : m_DubinsPaths) {
        if (p.containsTime(s.time())) {
            p.sample(s);
            return;
        }
    }
    std::stringstream stream;
    stream << "Requested time " << std::to_string(s.time()) << " outside DubinsPlan bounds, which spans from "
    << std::to_string(getStartTime()) << " to " << std::to_string(getEndTime());
    throw std::runtime_error(stream.str());
}

DubinsPlan::DubinsPlan(const State& s1, const State& s2, double rho) {
    m_DubinsPaths.emplace_back(s1, s2, rho);
}

bool DubinsPlan::empty() const {
    return m_DubinsPaths.empty();
}

std::vector<State> DubinsPlan::getHalfSecondSamples() const {
    // should check for duplicates
    std::vector<State> result;
    if (empty()) return result;
    State s;
    for (double time = getStartTime(); time < getEndTime(); time += planTimeDensity()) {
        s.time() = time;
        sample(s);
        result.push_back(s);
    }
    return result;
}

const std::vector<DubinsWrapper>& DubinsPlan::get() const {
    return m_DubinsPaths;
}

double DubinsPlan::totalTime() const {
    if (empty()) return 0;
    return m_DubinsPaths.back().getEndTime() - m_DubinsPaths.front().getStartTime();
}

bool DubinsPlan::containsTime(double time) const {
    for (const auto& p : m_DubinsPaths) if (p.containsTime(time)) return true;
    return false;
}

double DubinsPlan::getStartTime() const {
    if (m_DubinsPaths.empty()) throw std::runtime_error("Cannot access empty plan");
    return m_DubinsPaths.front().getStartTime();
}

double DubinsPlan::getEndTime() const {
    if (m_DubinsPaths.empty()) throw std::runtime_error("Cannot access empty plan");
    return m_DubinsPaths.back().getEndTime();
}

void DubinsPlan::changeIntoSuffix(double startTime) {
    if (m_DubinsPaths.empty()) throw std::runtime_error("Cannot access empty plan");
//    for (auto& p : m_DubinsPaths) {
//        if (p.containsTime(startTime)) {
//            p.updateStartTime(startTime);
//        }
//    }
    // drop segments now in the past
    while (m_DubinsPaths.front().getEndTime() < startTime) {
        m_DubinsPaths.erase(m_DubinsPaths.begin()); // yucky operation but it happens only rarely
    }
}

bool DubinsPlan::dangerous() const {
    return m_Dangerous;
}

void DubinsPlan::setDangerous(bool dangerous) {
    m_Dangerous = dangerous;
}
