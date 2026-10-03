#include "PresetBrowser.h"
#include "PluginProcessor.h"

static const char* kBanks[] = { "All", "Bass", "Lead", "Pad", "Orch", "Keys", "Arp", "FX", "Clsc", "User" };

PresetBrowser::PresetBrowser(Ersa8Processor& p, ErsaLookAndFeel& l)
    : proc(p), lnf(l)
{
    setLookAndFeel(&lnf);

    search.setLookAndFeel(&lnf);
    search.setTextToShowWhenEmpty("Search presets...", ErsaColors::textDim.withAlpha(0.7f));
    search.setColour(juce::TextEditor::backgroundColourId, ErsaColors::comboBg);
    search.setColour(juce::TextEditor::textColourId, ErsaColors::text);
    search.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3d43));
    search.setColour(juce::TextEditor::focusedOutlineColourId, ErsaColors::orange);
    search.setFont(lnf.uiFont(13.0f));
    search.addListener(this);
    addAndMakeVisible(search);

    for (int b = 0; b < 10; ++b)
    {
        auto* btn = new juce::TextButton(kBanks[b]);
        bankBtns.emplace_back(btn);
        addAndMakeVisible(btn);
        btn->setColour(juce::TextButton::textColourOffId, ErsaColors::textDim);
        btn->setColour(juce::TextButton::textColourOnId, ErsaColors::orangeHi);
        btn->onClick = [this, b] { activeBank = b; refreshBankButtons(); rebuildRows(); };
    }
    refreshBankButtons();

    list.setLookAndFeel(&lnf);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setRowHeight(26);
    addAndMakeVisible(list);
    auto& sb = list.getVerticalScrollBar();
    sb.setColour(juce::ScrollBar::thumbColourId, ErsaColors::orange.withAlpha(0.7f));
    sb.setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    sb.setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);

    closeBtn.onClick = [&] { if (onClose) onClose(); };
    saveBtn.onClick = [&]
    {
        juce::String name = search.getText().trim();
        if (name.isEmpty())
        {
            static int n = 1;
            name = "My Preset " + juce::String(n++);
        }
        if (saveUserPreset(name, proc.apvts))
        {
            activeBank = 9;
            refreshBankButtons();
            rebuildRows();
            search.setText("", false);
        }
    };
    delBtn.onClick = [&]
    {
        int r = list.getSelectedRow();
        if (r < 0 || r >= (int)rows.size()) return;
        if (rows[(size_t)r].isUser)
        {
            if (deleteUserPreset(rows[(size_t)r].file))
                rebuildRows();
        }
        else if (rows[(size_t)r].shadowed)
        {
            if (deleteFactoryShadow(rows[(size_t)r].index)) // restore shipped file
                rebuildRows();
        }
    };
    overBtn.onClick = [&]
    {
        int r = list.getSelectedRow();
        if (r < 0 || r >= (int)rows.size()) return;
        if (!rows[(size_t)r].isUser)
        {
            if (overwriteFactoryPreset(rows[(size_t)r].index, proc.apvts))
                rebuildRows();
        }
    };
    for (auto* b : { &closeBtn, &saveBtn, &overBtn, &delBtn })
    {
        addAndMakeVisible(b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c2c33));
        b->setColour(juce::TextButton::textColourOnId, ErsaColors::orange);
        b->setColour(juce::TextButton::textColourOffId, ErsaColors::orange);
    }
    rebuildRows();
}

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        currentFactory = proc.getCurrentProgram();
        rebuildRows();
        search.grabKeyboardFocus();
    }
}

void PresetBrowser::refreshBankButtons()
{
    for (size_t b = 0; b < bankBtns.size(); ++b)
    {
        bool on = ((int)b == activeBank);
        bankBtns[b]->setColour(juce::TextButton::textColourOffId,
                               on ? ErsaColors::orangeHi : ErsaColors::textDim);
    }
}

void PresetBrowser::rebuildRows()
{
    rows.clear();
    juce::String q = search.getText().trim().toLowerCase();
    if (activeBank == 9)
    {
        for (auto& u : scanUserPresets())
        {
            if (q.isNotEmpty() && !u.name.toLowerCase().contains(q)) continue;
            rows.push_back({ true, -1, u.name, u.file });
        }
    }
    else
    {
        const auto& all = getFactoryPresets();
        for (size_t k = 0; k < all.size(); ++k)
        {
            auto& pr = all[k];
            if (activeBank != 0 && (1 + juce::jlimit(0, 7, pr.index / 16)) != activeBank) continue;
            if (q.isNotEmpty() && !pr.name.toLowerCase().contains(q)) continue;
            Row r;
            r.isUser = false; r.index = pr.index;
            r.name = pr.name + (pr.shadowed ? juce::String(" *") : juce::String());
            r.file = pr.file; r.shadowed = pr.shadowed;
            rows.push_back(r);
        }
    }
    list.updateContent();
    list.repaint();
}

int PresetBrowser::getNumRows() { return (int)rows.size(); }

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int)rows.size()) return;
    auto& r = rows[(size_t)row];
    bool isCurrent = (!r.isUser && r.index == currentFactory);
    if (selected)
    {
        g.setColour(ErsaColors::orange.withAlpha(0.22f));
        g.fillRoundedRectangle(2, 1, w - 4, h - 2, 3.0f);
    }
    g.setColour(isCurrent ? ErsaColors::orangeHi : ErsaColors::textDim.withAlpha(0.8f));
    g.setFont(lnf.uiFont(12.0f));
    juce::String num = r.isUser ? juce::String("USR")
                                : juce::String(r.index).paddedLeft('0', 3);
    g.drawText(num, 10, 0, 44, h, juce::Justification::centredLeft, false);
    g.setColour(isCurrent ? ErsaColors::orangeHi : ErsaColors::text);
    g.setFont(lnf.uiFont(13.0f));
    g.drawText(r.name, 58, 0, w - 66, h, juce::Justification::centredLeft, true);
}

void PresetBrowser::selectedRowsChanged(int lastRow)
{
    juce::ignoreUnused(lastRow);
    int r = list.getSelectedRow();
    if (r < 0 || r >= (int)rows.size()) return;
    auto& row = rows[(size_t)r];
    if (row.isUser)
    {
        if (loadUserPreset(row.file, proc.apvts))
        {
            currentFactory = -1;
            lastLoadedName = row.name;
            lastLoadedIsUser = true;
        }
    }
    else
    {
        proc.loadPreset(row.index);
        currentFactory = row.index;
        lastLoadedName = {};
        lastLoadedIsUser = false;
    }
    if (onPresetChanged) onPresetChanged();
    list.repaint();
}

void PresetBrowser::listBoxItemDoubleClicked(int, const juce::MouseEvent&)
{
    if (onClose) onClose();
}

void PresetBrowser::textEditorTextChanged(juce::TextEditor&) { rebuildRows(); }

void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.55f)); // scrim
    g.setColour(ErsaColors::panel);
    g.fillRoundedRectangle(panel.toFloat(), 6.0f);
    g.setColour(ErsaColors::border);
    g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 6.0f, 1.0f);
    g.setColour(ErsaColors::bar);
    g.fillRoundedRectangle(panel.getX(), panel.getY(), panel.getWidth(), 30, 6.0f);
    g.fillRect(panel.getX(), panel.getY() + 24, panel.getWidth(), 6);
    g.setColour(ErsaColors::barText);
    g.setFont(lnf.titleFont(14.0f));
    g.drawText("PRESETS", panel.getX() + 12, panel.getY(), 200, 30,
               juce::Justification::centredLeft, false);
}

void PresetBrowser::resized()
{
    panel = juce::Rectangle<int>(300, 46, 640, 494);
    closeBtn.setBounds(panel.getRight() - 40, panel.getY() + 4, 32, 22);
    search.setBounds(panel.getX() + 12, panel.getY() + 40, panel.getWidth() - 24, 26);
    int bw = (panel.getWidth() - 24 - 9 * 6) / 10;
    for (size_t b = 0; b < bankBtns.size(); ++b)
        bankBtns[b]->setBounds(panel.getX() + 12 + (int)b * (bw + 6), panel.getY() + 72, bw, 22);
    list.setBounds(panel.getX() + 12, panel.getY() + 102, panel.getWidth() - 24, 316);
    saveBtn.setBounds(panel.getX() + 12, panel.getBottom() - 34, 90, 24);
    overBtn.setBounds(panel.getX() + 108, panel.getBottom() - 34, 90, 24);
    delBtn.setBounds(panel.getX() + 204, panel.getBottom() - 34, 70, 24);
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    if (!panel.contains(e.getPosition()) && onClose) onClose(); // scrim click closes
}
