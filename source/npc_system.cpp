#include "npc_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace m3d {
namespace {

std::string jsonEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': break;
        default: result.push_back(character); break;
        }
    }
    return result;
}

} // namespace

void NpcSystem::configure(WorldTheme theme)
{
    theme_ = theme;
    dialogue_ = {};
    placeholderCursor_ = 0;
    if (theme == WorldTheme::GreekHeaven) {
        npcs_ = {
            {"thalia", "THALIA", "ORÁCULO DEL OLIMPO",
             "Oráculo sereno que interpreta señales del cielo griego.", {-5.5f,0.13f,-3.0f}, 0.4f, 5, 0.36f,
             {"Las nubes hoy forman un camino. Quizá sea para ti.", "Los dioses guardan silencio, pero el agua nunca deja de responder.", "Busca el portal de mármol cuando el cielo cambie de color."}},
            {"nikandros", "NIKANDROS", "JARDINERO DE LAURELES",
             "Cuidador alegre de los jardines y laureles del Olimpo.", {5.5f,0.13f,4.0f}, -1.8f, 11, 0.35f,
             {"Cada laurel recuerda el nombre de un viajero.", "No pises las flores doradas; todavía están soñando.", "Las islas flotantes beben de la misma cascada."}},
            {"ione", "IONE", "GUARDIANA DE LAS NUBES",
             "Guardiana vigilante de los puentes y nubes de mármol.", {-7.0f,0.13f,8.0f}, 1.1f, 15, 0.35f,
             {"Puedo ver tres islas desde aquí, aunque mañana quizá sean cuatro.", "Las nubes de mármol son firmes si caminas sin miedo.", "El viento trae noticias desde la Ciudad Romana."}},
            {"dorian", "DORIAN", "CUSTODIO DEL TEMPLO",
             "Custodio formal del templo blanco y sus antiguas reglas.", {7.5f,0.13f,-7.0f}, -0.7f, 16, 0.38f,
             {"El templo está abierto para quien llegue con buenas intenciones.", "La estatua central no representa a un dios, sino a una promesa.", "Mantén despejado el sendero; pronto llegarán más viajeros."}},
        };
    } else {
        npcs_ = {
            {"lucia", "LUCIA", "CRONISTA DEL FORO",
             "Cronista curiosa que conoce historias y rumores de la Ciudad Romana.", {-5.5f,0.13f,5.0f}, 0.6f, 1, 0.35f,
             {"El foro parece tranquilo, pero cada columna guarda un rumor.", "Dicen que el acueducto canta cuando la ciudad duerme.", "La fuente lateral fue movida para dejar libre la plaza central."}},
            {"marcus", "MARCUS", "GUARDIA DE LA PUERTA",
             "Guardia disciplinado que protege las puertas y orienta a los visitantes.", {6.5f,0.13f,6.0f}, -2.2f, 10, 0.38f,
             {"Las puertas están abiertas. Mantén tu arma guardada dentro del foro.", "El camino del sur conduce al acueducto.", "He visto comerciantes llegar desde todos los distritos."}},
            {"flavia", "FLAVIA", "MERCADERA DE TELAS",
             "Mercadera carismática que comenta precios, moda y noticias locales.", {-8.0f,0.13f,-5.0f}, 1.0f, 4, 0.34f,
             {"Hoy tengo lino, púrpura y una tela que cambia con la luz.", "Tu atuendo llamaría la atención incluso en el Senado.", "Vuelve mañana; siempre encuentro algo distinto."}},
            {"cassius", "CASSIUS", "INGENIERO DEL ACUEDUCTO",
             "Ingeniero práctico obsesionado con agua, arcos y mantenimiento.", {8.0f,0.13f,-7.0f}, -0.8f, 12, 0.36f,
             {"Cada arco distribuye el peso hacia el siguiente. Es simple y perfecto.", "El canal superior necesita otra inspección antes del anochecer.", "Sin agua no hay ciudad, solo piedra caliente."}},
        };
    }
}

std::vector<DynamicCollider> NpcSystem::colliders() const
{
    std::vector<DynamicCollider> result;
    result.reserve(npcs_.size());
    for (const auto& npc : npcs_) result.push_back({npc.position, npc.colliderRadius, 1.75f});
    return result;
}

const NpcDefinition* NpcSystem::nearest(const Vec3& playerPosition, float maxDistance) const
{
    const NpcDefinition* best = nullptr;
    float bestDistanceSquared = maxDistance * maxDistance;
    for (const auto& npc : npcs_) {
        const float dx = npc.position.x - playerPosition.x;
        const float dz = npc.position.z - playerPosition.z;
        const float distanceSquared = dx * dx + dz * dz;
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            best = &npc;
        }
    }
    return best;
}

std::string NpcSystem::placeholderFor(const NpcDefinition& npc)
{
    if (npc.placeholders.empty()) return "Todavía no tengo nada que decir.";
    const std::string result = npc.placeholders[placeholderCursor_ % npc.placeholders.size()];
    ++placeholderCursor_;
    return result;
}

std::string NpcSystem::requestJson(const NpcInferenceRequest& request) const
{
    std::ostringstream output;
    output << "{\"protocol\":1,\"request_id\":" << request.requestId
           << ",\"npc_id\":\"" << jsonEscape(request.npcId)
           << "\",\"npc_name\":\"" << jsonEscape(request.npcName)
           << "\",\"persona\":\"" << jsonEscape(request.persona)
           << "\",\"player_text\":\"" << jsonEscape(request.playerText)
           << "\",\"world\":\"" << jsonEscape(request.world) << "\"}";
    return output.str();
}

bool NpcSystem::interact(const Vec3& playerPosition, std::string_view playerText)
{
    const NpcDefinition* npc = nearest(playerPosition);
    if (!npc) return false;

    std::string response;
    if (inferenceCallback_) {
        const NpcInferenceRequest request{
            nextRequestId_++, npc->id, npc->name, npc->aiPersona,
            std::string(playerText), std::string(worldName(theme_))
        };
        const std::string json = requestJson(request);
        std::array<char, 2048> buffer{};
        if (inferenceCallback_(json.c_str(), buffer.data(), buffer.size(), inferenceUserData_)) {
            response = buffer.data();
        }
    }
    if (response.empty()) response = placeholderFor(*npc);
    dialogue_ = {true, npc->id, npc->name, npc->title, std::move(response)};
    return true;
}

} // namespace m3d
