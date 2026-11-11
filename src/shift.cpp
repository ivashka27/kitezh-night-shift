#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>

#include <set>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::set<std::string> seen_signs;  // запишем, какие позывные уже были
    std::vector<Vessel> valid_queue;   // запихаем хорошие суда

    for (int i = 0; i < (int)queue.size(); i++) {
        const Vessel& vessel = queue[i];

        // 1.проверка на дповтор
        if (seen_signs.find(vessel.call_sign) != seen_signs.end()) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
            continue;
        }
        seen_signs.insert(vessel.call_sign);  // запоминаем позывной

        // 2.проверка на ошибки в данных
        if (validateVessel(vessel).has_value()) {
            report.refused.push_back({vessel.call_sign, "invalid"});
            continue;
        }

        // 3.проверка осадки (больше 28 не пройдёт)
        if (vessel.draft_dm > 28) {
            report.refused.push_back({vessel.call_sign, "draft"});
            continue;
        }

        // Если все хорошо добавляем его в список хороших
        valid_queue.push_back(vessel);
    }
    Lock lock;  // создаём шлюз

    while (!valid_queue.empty()) {
        // Берём первое судно как шаблон
        Vessel base = valid_queue[0];
        std::vector<Vessel> batch;
        batch.push_back(base);

        std::vector<bool> used(valid_queue.size(), false);
        used[0] = true;

        Direction batch_direction = base.direction;

        if (base.kind != VesselKind::Oversized) {
            bool ambulance_was_skipped = false;

            for (int i = 1; i < (int)(valid_queue.size()); i++) {
                const Vessel& candidate = valid_queue[i];

                bool should_take = true; // По умолчанию берем

                if (candidate.direction != batch_direction) {
                    should_take = false;
                }
                if (candidate.kind == VesselKind::Oversized) {
                    should_take = false;
                }
                if (!fitsInChamber(batch, candidate)) {
                    should_take = false;
                }
                if (ambulance_was_skipped && candidate.kind != VesselKind::Ambulance) {
                    should_take = false;
                }

                if (should_take) {
                    batch.push_back(candidate);
                    used[i] = true;
                } else {
                    // Если не взяли по любой причине — поднимаем флаг
                    ambulance_was_skipped = true;
                }
            }
        }

        // Убираем из очереди тех, кто вошёл в партию
        std::vector<Vessel> remaining;
        for (int i = 0; i < (int)(valid_queue.size()); i++) {
            if (!used[i]) {
                remaining.push_back(valid_queue[i]);
            }
        }
        valid_queue = remaining;

        // 1. Оба затвора закроем
        lock.close(Gate::Lower);
        lock.close(Gate::Upper);

        // 2. Решаем с какого уровня и до какого, куда заходить и откуда выходить
        Level entry_level;
        Gate  entry_gate;
        Level exit_level;
        Gate  exit_gate;

        if (batch_direction == Direction::Upstream) {
            // Идём вверх по реке: заходим снизу, выпускаем сверху
            entry_level = Level::Low;
            entry_gate  = Gate::Lower;
            exit_level  = Level::High;
            exit_gate   = Gate::Upper;
        } else {
            // Идём вниз по реке: заходим сверху, выпускаем снизу
            entry_level = Level::High;
            entry_gate  = Gate::Upper;
            exit_level  = Level::Low;
            exit_gate   = Gate::Lower;
        }

        // 3. Выравниваем воду до входного
        while (lock.level() != entry_level) {
            lock.pump();
        }

        // 4. Открываем вход, заводим суда, закрываем вход
        lock.open(entry_gate);
        for (int i = 0; i < (int)(batch.size()); i++) {
            lock.enter(batch[i]);
        }
        lock.close(entry_gate);

        // 5. Выравниваем воду до выходного уровня
        while (lock.level() != exit_level) {
            lock.pump();
        }

        // 6. Открываем выход, выпускаем суда, закрываем выход
        lock.open(exit_gate);
        for (int i = 0; i < (int)batch.size(); i++) {
            const std::string& sign = batch[i].call_sign;
            lock.exit(sign);
            report.passed.push_back(sign);
        }
        lock.close(exit_gate);
    }
    report.pump_cycles = lock.pumpCycles();

    return report;
}

}  // namespace kitezh
