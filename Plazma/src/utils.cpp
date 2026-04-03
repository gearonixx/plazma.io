#include "utils.h"

#include <QQmlApplicationEngine>

#include <ranges>

namespace ranges = std::ranges;

Utils::Utils(QQmlApplicationEngine* engine) : m_engine(engine) { s_instance = this; }

Utils* Utils::instance() { return s_instance; }
