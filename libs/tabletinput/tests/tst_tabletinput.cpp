#include <tabletinput/PenReader.h>
#include <tabletinput/TouchRing.h>

#include <QTest>

using tabletinput::DeviceRects;
using tabletinput::PenSample;

namespace {

// Rectángulos que informó el driver en la desktop (HU-43): tableta 32513 × 20321
// himétricos → los dos monitores, 4480 × 1080 px.
DeviceRects desktopRects()
{
    DeviceRects r;
    r.pointer = {0, 0, 32513, 20321};
    r.display = {0, 0, 4480, 1080};
    return r;
}

POINTER_PEN_INFO pen(LONG himX, LONG himY, UINT32 pressure, UINT64 perfCount, bool contact = true)
{
    POINTER_PEN_INFO p{};
    p.pointerInfo.pointerType = PT_PEN;
    p.pointerInfo.ptHimetricLocation = {himX, himY};
    p.pointerInfo.PerformanceCount = perfCount;
    p.pointerInfo.pointerFlags = contact ? POINTER_FLAG_INCONTACT : 0;
    p.penMask = PEN_MASK_PRESSURE | PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
    p.pressure = pressure;
    return p;
}

constexpr int64_t kQpcHz = 10000000; // el de las dos máquinas (HU-43)

} // namespace

class TestTabletInput : public QObject {
    Q_OBJECT

private slots:
    void lapizVerticalTieneAltitud90()
    {
        float az = -1, alt = -1;
        tabletinput::tiltToAzimuthAltitude(0, 0, az, alt);
        QCOMPARE(alt, 90.0f);
        QCOMPARE(az, 0.0f);
    }

    void inclinacionAAzimutYAltitud()
    {
        float az, alt;
        tabletinput::tiltToAzimuthAltitude(45, 0, az, alt); // inclinado hacia +x
        QVERIFY(qAbs(az - 0.0f) < 0.01f);
        QVERIFY(qAbs(alt - 45.0f) < 0.01f);

        tabletinput::tiltToAzimuthAltitude(0, 30, az, alt); // hacia +y (abajo en pantalla)
        QVERIFY(qAbs(az - 90.0f) < 0.01f);
        QVERIFY(qAbs(alt - 60.0f) < 0.01f);

        tabletinput::tiltToAzimuthAltitude(-30, 0, az, alt); // hacia −x
        QVERIFY(qAbs(az - 180.0f) < 0.01f);

        tabletinput::tiltToAzimuthAltitude(0, -30, az, alt); // hacia −y: azimut en [0, 360)
        QVERIFY(qAbs(az - 270.0f) < 0.01f);
    }

    // HU-43: ≈ 7,26 himétricos por píxel con este mapeo, y ptHimetricLocation da
    // posiciones con decimales (no cuantizadas al píxel).
    void himetricoAPixelConDecimales()
    {
        const PenSample a = tabletinput::normalize(pen(8000, 10000, 512, 0), desktopRects(), kQpcHz);
        QVERIFY(qAbs(a.x - 8000.0 * 4480 / 32513) < 1e-9);
        QVERIFY(qAbs(a.y - 10000.0 * 1080 / 20321) < 1e-9);
        const PenSample b = tabletinput::normalize(pen(8001, 10000, 512, 0), desktopRects(), kQpcHz);
        const double paso = b.x - a.x;
        QVERIFY2(paso > 0 && paso < 0.2, qPrintable(QString::number(paso)));
    }

    void presionYContacto()
    {
        PenSample s = tabletinput::normalize(pen(0, 0, 512, 0), desktopRects(), kQpcHz);
        QCOMPARE(s.pressure, 0.5f);
        QVERIFY(s.inContact);

        s = tabletinput::normalize(pen(0, 0, 0, 0, false), desktopRects(), kQpcHz);
        QVERIFY(!s.inContact);
        QCOMPARE(s.pressure, 0.0f);

        // Sin presión en la máscara: 1 en contacto.
        POINTER_PEN_INFO p = pen(0, 0, 0, 0);
        p.penMask = 0;
        QCOMPARE(tabletinput::normalize(p, desktopRects(), kQpcHz).pressure, 1.0f);
    }

    void relojEnMicrosegundos()
    {
        QCOMPARE(tabletinput::qpcToMicroseconds(10000000, kQpcHz), int64_t(1000000));
        QCOMPARE(tabletinput::qpcToMicroseconds(12345, kQpcHz), int64_t(1234));
        // Contador grande (días encendida) sin desbordar: 30 días a 10 MHz.
        const int64_t treintaDias = int64_t(30) * 24 * 3600 * kQpcHz;
        QCOMPARE(tabletinput::qpcToMicroseconds(treintaDias + 7, kQpcHz), int64_t(30) * 24 * 3600 * 1000000);
        // Otra frecuencia.
        QCOMPARE(tabletinput::qpcToMicroseconds(3000, 3000), int64_t(1000000));
    }

    // GetPointerPenInfoHistory entrega de la más nueva a la más vieja.
    void historialEnOrdenCronologico()
    {
        const POINTER_PEN_INFO historia[3] = {pen(300, 0, 512, 30000), pen(200, 0, 512, 20000), pen(100, 0, 512, 10000)};
        std::vector<PenSample> out{PenSample{}}; // se agrega al final, sin borrar
        tabletinput::appendHistory(historia, 3, desktopRects(), kQpcHz, out);
        QCOMPARE(out.size(), size_t(4));
        QCOMPARE(out[1].timeUs, int64_t(1000));
        QCOMPARE(out[2].timeUs, int64_t(2000));
        QCOMPARE(out[3].timeUs, int64_t(3000));
        QVERIFY(out[1].x < out[2].x && out[2].x < out[3].x);
    }

    void gomaBotonYRotacion()
    {
        POINTER_PEN_INFO p = pen(0, 0, 512, 0);
        QVERIFY(!tabletinput::normalize(p, desktopRects(), kQpcHz).eraser);
        QVERIFY(!tabletinput::normalize(p, desktopRects(), kQpcHz).rotation.has_value());

        p.penFlags = PEN_FLAG_INVERTED; // goma en proximidad
        QVERIFY(tabletinput::normalize(p, desktopRects(), kQpcHz).eraser);
        p.penFlags = PEN_FLAG_ERASER; // goma apoyada
        QVERIFY(tabletinput::normalize(p, desktopRects(), kQpcHz).eraser);
        p.penFlags = PEN_FLAG_BARREL;
        const PenSample s = tabletinput::normalize(p, desktopRects(), kQpcHz);
        QVERIFY(s.barrel && !s.eraser);

        p.penMask |= PEN_MASK_ROTATION;
        p.rotation = 45;
        QCOMPARE(tabletinput::normalize(p, desktopRects(), kQpcHz).rotation.value_or(-1), 45.0f);
    }

    // Rueda (HU-74), con lo medido en la Intuos4: posiciones 1 a 72, suben en el sentido de
    // las agujas del reloj, 0 al levantar el dedo.
    void ruedaGiraLoQueSeDesliza()
    {
        using tabletinput::RingTracker;
        RingTracker ring;
        int64_t t = 0;
        const auto feed = [&](uint32_t p) { return ring.feed(p, t += 25000); };
        QCOMPARE(feed(70).degrees, 0.0); // apoyar no gira
        QCOMPARE(feed(71).degrees, 5.0);
        QCOMPARE(feed(72).degrees, 5.0);
        QCOMPARE(feed(1).degrees, 5.0); // cruza de 72 a 1 sin saltar
        QCOMPARE(feed(3).degrees, 10.0); // se saltea una posición si el dedo va rápido
        QCOMPARE(feed(2).degrees, -5.0); // al revés, negativo
        QCOMPARE(feed(1).degrees, -5.0);
        QCOMPARE(feed(72).degrees, -5.0);
        const RingTracker::Step up = feed(0);
        QVERIFY(up.ended);
        QCOMPARE(up.degrees, 0.0);
        QVERIFY(!feed(0).ended); // un solo aviso por levantada

        // Apoyar en otro lado, después de soltar: no salta hasta ahí.
        t += 1000000;
        QCOMPARE(feed(30).degrees, 0.0);
        QCOMPARE(feed(31).degrees, 5.0);
    }

    // Un 0 suelto en medio de una vuelta (el dedo se despega apenas) no corta el giro: se
    // sigue desde donde estaba. Pero si pasó tiempo o el dedo volvió lejos, es otro toque.
    void ruedaIgnoraRebotes()
    {
        using tabletinput::RingTracker;
        RingTracker ring;
        QCOMPARE(ring.feed(60, 0).degrees, 0.0);
        QCOMPARE(ring.feed(62, 25000).degrees, 10.0);
        QVERIFY(ring.feed(0, 50000).ended);
        QCOMPARE(ring.feed(63, 75000).degrees, 5.0);   // rebote: sigue
        QVERIFY(ring.feed(0, 100000).ended);
        QCOMPARE(ring.feed(20, 125000).degrees, 0.0);  // lejos: otro toque
        QVERIFY(ring.feed(0, 150000).ended);
        QCOMPARE(ring.feed(21, 500000).degrees, 0.0);  // tarde: otro toque
    }

    // PenReader ignora todo lo que no sea WM_POINTER de un lápiz.
    void otrosMensajesNoSeConsumen()
    {
        tabletinput::PenReader reader;
        std::vector<PenSample> out;
        QVERIFY(!reader.handleMessage(nullptr, WM_MOUSEMOVE, 0, 0, out));
        QVERIFY(!reader.handleMessage(nullptr, WM_POINTERUPDATE, 9999, 0, out)); // puntero inexistente
        QVERIFY(out.empty());
    }
};

QTEST_GUILESS_MAIN(TestTabletInput)
#include "tst_tabletinput.moc"
