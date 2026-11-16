#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mpcui
{
inline juce::Colour ink() { return juce::Colour (0xff2b2a27); }

// Draws text squeezed horizontally (down to minScale) so it fits the rectangle; ellipsis beyond that.
inline void fitText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j,
                     juce::Font f, float minScale = 0.72f)
{
    const float w = f.getStringWidthFloat (t);
    if (w > (float) r.getWidth()) f.setHorizontalScale (juce::jmax (minScale, (float) r.getWidth() / w));
    g.setFont (f);
    g.drawText (t, r, j, true);
}
}

// Rotary knob (vertical drag), value 0..1.
class Knob : public juce::Component
{
public:
    std::function<void (float)> onChange;
    float value = 0.5f;
    juce::Colour cap { 0xffd8d5c8 };
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (7);
        const auto c = b.getCentre(); const float R = b.getWidth() * 0.5f;
        g.setColour (mpcui::ink());
        for (int i = 0; i <= 10; ++i)                      // scale dots
        {
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) i);
            g.fillEllipse (c.x + std::sin (a) * (R + 5) - 1.2f, c.y - std::cos (a) * (R + 5) - 1.2f, 2.4f, 2.4f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a48), c.x - R, c.y - R, juce::Colour (0xff0e0e0e), c.x + R, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);   // black skirt
        const float r = R * 0.72f;
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.4f), c.x - r * 0.6f, c.y - r * 0.7f, cap.darker (0.55f), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        const float a = juce::degreesToRadians (-135.0f + 270.0f * value);
        g.setColour (juce::Colours::black);
        g.drawLine (c.x + std::sin (a) * r * 0.25f, c.y - std::cos (a) * r * 0.25f, c.x + std::sin (a) * R * 0.98f, c.y - std::cos (a) * R * 0.98f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (c.x + std::sin (a) * r * 0.3f, c.y - std::cos (a) * r * 0.3f, c.x + std::sin (a) * R * 0.95f, c.y - std::cos (a) * R * 0.95f, 1.6f);
    }
    void mouseDown (const juce::MouseEvent& e) override { startY = e.y; startV = value; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        value = juce::jlimit (0.0f, 1.0f, startV + (float) (startY - e.y) / 150.0f);
        if (onChange) onChange (value);
        repaint();
    }
private:
    int startY = 0; float startV = 0;
};

// Hardware push button. Label can sit above the button (offset `off` between the label centre and the button top).
class HwButton : public juce::Component
{
public:
    enum Style { Cream, Key, Inside, Red, Green, Dark };
    std::function<void()> onClick;
    juce::String text, label;
    Style style = Cream;
    int off = 15;                 // label centre is `off` px above the button top
    bool led = false, hasLed = false;

    float topPad() const { return label.isEmpty() ? 0.0f : (float) (off + 8); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto btn = b.reduced (14.0f, 0).withTrimmedTop (topPad());
        if (label.isNotEmpty())
        {
            g.setColour (mpcui::ink());
            mpcui::fitText (g, label, juce::Rectangle<float> (0, 0, b.getWidth(), 16).toNearestInt(), juce::Justification::centred,
                            juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        }
        if (hasLed)
        {
            g.setColour (led ? juce::Colour (0xffffe14a) : juce::Colour (0xff3b3b38));
            g.fillEllipse (btn.getCentreX() - 3.0f, btn.getY() - 7.0f, 6.0f, 6.0f);
        }
        juce::Colour top, bot, txt = mpcui::ink();
        switch (style)
        {
            case Cream: case Inside: top = juce::Colour (0xffe9e5d6); bot = juce::Colour (0xffbab59f); break;
            case Key:   top = juce::Colour (0xff75756f); bot = juce::Colour (0xff4b4b47); break;
            case Dark:  top = juce::Colour (0xff6f6f69); bot = juce::Colour (0xff3f3f3b); break;
            case Red:   top = juce::Colour (0xffa83a30); bot = juce::Colour (0xff6e1a14); txt = juce::Colours::white; break;
            case Green: top = juce::Colour (0xffb4dcaa); bot = juce::Colour (0xff86b67c); break;
        }
        if (down) std::swap (top, bot);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (btn.translated (0, 1.5f), 3.5f);
        g.setGradientFill (juce::ColourGradient (top, 0, btn.getY(), bot, 0, btn.getBottom(), false));
        g.fillRoundedRectangle (btn.reduced (0, 0), 3.5f);
        g.setColour (juce::Colour (0xff35342f)); g.drawRoundedRectangle (btn.reduced (0.5f), 3.5f, 1.3f);
        if (text.isNotEmpty())
        {
            g.setColour (txt);
            g.setFont (juce::Font (juce::FontOptions (style == Inside ? 11.0f : 15.0f, juce::Font::bold)));
            g.drawFittedText (text, btn.reduced (1).toNearestInt(), juce::Justification::centred, 2, 0.55f);
        }
    }
    bool hitTest (int x, int y) override
    { return getLocalBounds().toFloat().reduced (14.0f, 0).withTrimmedTop (topPad()).contains ((float) x, (float) y); }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false; repaint();
        if (hitTest (e.x, e.y) && onClick) onClick();
    }
private:
    bool down = false;
};

// DATA wheel: drag (vertical or horizontal) to emit steps.
class DataWheel : public juce::Component
{
public:
    std::function<void (int)> onTurn;
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2);
        const auto c = b.getCentre(); const float R = b.getWidth() * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (b.translated (2, 3));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5a58), c.x - R, c.y - R, juce::Colour (0xff111111), c.x + R, c.y + R, false));
        g.fillEllipse (b);
        const float r = R * 0.9f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9b9b6), c.x - r * 0.5f, c.y - r * 0.8f, juce::Colour (0xff4c4c4a), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        const float dx = c.x + std::sin (angle) * r * 0.45f, dy = c.y - std::cos (angle) * r * 0.45f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a29), dx, dy, juce::Colour (0xff9a9a97), dx + 10, dy + 14, false));
        g.fillEllipse (dx - 14, dy - 14, 28, 28);
    }
    void mouseDown (const juce::MouseEvent& e) override { last = e.getPosition(); acc = 0; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto d = e.getPosition() - last; last = e.getPosition();
        const float delta = (float) (d.x - d.y);
        angle += delta * 0.03f; acc += delta;
        while (acc >= 6.0f)  { acc -= 6.0f; if (onTurn) onTurn (1); }
        while (acc <= -6.0f) { acc += 6.0f; if (onTurn) onTurn (-1); }
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    { if (onTurn && w.deltaY != 0) onTurn (w.deltaY > 0 ? 1 : -1); angle += w.deltaY; repaint(); }
private:
    juce::Point<int> last; float angle = 0, acc = 0;
};

// Vertical fader, value 0..1 (1 = top).
class Fader : public juce::Component
{
public:
    std::function<void (float)> onChange;
    float value = 0.0f;
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX();
        g.setColour (juce::Colour (0xff111111)); g.fillRoundedRectangle (cx - 3, 6, 6, b.getHeight() - 12, 2.5f);
        const float y = 14.0f + (1.0f - value) * (b.getHeight() - 28.0f);
        auto cap = juce::Rectangle<float> (cx - 14, y - 15, 28, 30);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e5da), 0, cap.getY(), juce::Colour (0xff9a9684), 0, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (juce::Colour (0xff2b2a27)); g.drawRoundedRectangle (cap, 2.5f, 1.2f);
        g.drawLine (cap.getX() + 2, y, cap.getRight() - 2, y, 2.0f);
    }
    void mouseDown (const juce::MouseEvent& e) override { set (e); }
    void mouseDrag (const juce::MouseEvent& e) override { set (e); }
private:
    void set (const juce::MouseEvent& e)
    {
        value = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - 14.0f) / ((float) getHeight() - 28.0f));
        if (onChange) onChange (value);
        repaint();
    }
};
