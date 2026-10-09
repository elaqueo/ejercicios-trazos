#pragma once

#include "drymedia/Contact.h"
#include "drymedia/Medium.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace drymedia {

class Paper;

// Una muestra del lápiz en coordenadas de simulación: celdas del papel (la app convierte
// desde milímetros con kCellsPerMm), presión 0..1, azimut y altitud en grados.
struct PencilSample {
    double x = 0, y = 0;
    float pressure = 0;
    float azimuth = 0;
    float altitude = 90;
};

// Rectángulo de celdas que cambió [x0, x1) × [y0, y1).
struct DirtyRect {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool empty() const { return x1 <= x0 || y1 <= y0; }
    void unite(const DirtyRect& o);
};

// El lápiz sobre el papel (plan, "Contacto y depósito"). Entre dos muestras, la punta se
// barre sobre el segmento en subpasos de como mucho una celda, interpolando presión e
// inclinación; en cada subpaso deposita ∝ penetración × distancia × blandura ×
// (1 − saturación). Como el aporte es proporcional a la distancia, el resultado casi no
// depende de cuántas muestras tenga el trazo: no hay dabs.
//
// Determinismo: las posiciones pasan a punto fijo (1/256 de celda) al entrar, y la
// punta se cachea por ángulo redondeado a 1°, así el papel depende solo de la secuencia
// de muestras y los parámetros.
class Pencil {
public:
    Pencil(Paper& paper, const Medium& medium, Contact::Path path = Contact::Path::Auto);
    ~Pencil();
    Pencil(const Pencil&) = delete;
    Pencil& operator=(const Pencil&) = delete;

    void beginStroke(const PencilSample& sample);
    // Barre desde la muestra anterior hasta esta y devuelve las celdas que cambiaron.
    DirtyRect strokeTo(const PencilSample& sample);
    void endStroke();
    bool inStroke() const { return m_inStroke; }

    // Tiles (índice ty · tilesX + tx) que el trazo actual modificó, en el orden en que
    // los tocó por primera vez.
    const std::vector<int>& strokeTiles() const { return m_strokeTiles; }

    // Se llama justo antes de la primera escritura del trazo en cada tile, con el
    // contenido previo del tile (nullptr si el trazo lo va a crear). Lo usa el deshacer
    // (History, HU-53) para guardar el estado anterior.
    using TileObserver = std::function<void(int tileIndex, const uint16_t* before)>;
    void setTileObserver(TileObserver observer) { m_observer = std::move(observer); }

    // Cambia el medio (por ejemplo, para calibrar la blandura en vivo). Vale desde el
    // próximo segmento; vacía el caché de puntas.
    void setMedium(const Medium& medium);
    const Medium& medium() const { return m_medium; }

    // Desgaste de la mina (HU-62). Por defecto el lápiz tiene el suyo; la app puede darle uno
    // por dureza (cada lápiz guarda su punta). sharpen() vuelve a la punta cónica nueva.
    void setWear(LeadWear* wear);
    // Otro medio con su desgaste (la app, al cambiar de dureza: cada una tiene el suyo).
    void setMedium(const Medium& medium, LeadWear* wear);
    LeadWear& wear() { return *m_wear; }
    const LeadWear& wear() const { return *m_wear; }
    void sharpen();

    Contact::Path path() const { return m_contact.path(); }
    uint64_t substeps() const { return m_substeps; }

private:
    struct FixedSample {
        int64_t x = 0, y = 0; // celdas × 256
        float pressure = 0, azimuth = 0, altitude = 90;
    };
    static FixedSample toFixed(const PencilSample& s);
    const Tip& tipFor(float azimuth, float altitude);
    bool wears() const { return m_medium.kind == Medium::Kind::Lead && m_medium.wearRate > 0; }
    void setupWear();
    void clearTips();
    void wearAt(const Tip& tip, uint16_t k);
    DirtyRect depositAt(int64_t x, int64_t y, float pressure, const Tip& tip, uint16_t k, uint16_t kb,
                        uint16_t kd);

    Paper& m_paper;
    Medium m_medium;
    Contact m_contact;
    std::vector<std::unique_ptr<Tip>> m_tips; // caché: 360 azimuts × 91 altitudes
    std::vector<size_t> m_filledTips;         // los lugares del caché con punta
    LeadWear m_ownWear;
    LeadWear* m_wear = &m_ownWear;
    FixedSample m_last;
    bool m_inStroke = false;
    std::vector<int> m_strokeTiles;
    std::vector<bool> m_tileMarked;
    TileObserver m_observer;
    uint64_t m_substeps = 0;
};

} // namespace drymedia
