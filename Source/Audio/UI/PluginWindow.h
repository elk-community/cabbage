/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   27th April 2017).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "../Filters/FilterIOConfiguration.h"
#include "../Plugins/CabbagePluginEditor.h"


class FilterGraph;

//==============================================================================
/**
    A desktop window containing a plugin's GUI.
*/
class PluginWindow  : public DocumentWindow, public ChangeBroadcaster
{
public:
    enum class Type
    {
        normal = 0,
        generic,
        programs,
        audioIO,
        numTypes
    };

    PluginWindow (AudioProcessorGraph::Node* n, Type t, OwnedArray<PluginWindow>& windowList)
       : DocumentWindow (n->getProcessor()->getName(),
                         LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                         DocumentWindow::minimiseButton | DocumentWindow::closeButton),
         activeWindowList (windowList),
         node (n), type (t)
    {
        setSize (400, 300);
        setResizeLimits(10, 50, 3000, 3000);
        setLookAndFeel (&pluginWindowLookAndFeel);

        if (auto* ui = createProcessorEditor(*node->getProcessor(), type))
        {
            setContentOwned(ui, true);
            if( auto* cabbgeEditor = dynamic_cast<CabbagePluginEditor*>(ui))
            {
                setSize(cabbgeEditor->getWidth(), cabbgeEditor->getHeight());;
                setBackgroundColour(cabbgeEditor->titlebarColour); // <-- set titlebar colour of the plugin window
            
                pluginWindowLookAndFeel.titlebarContrastingGradient = cabbgeEditor->titlebarGradientAmount;
                if (cabbgeEditor->defaultFontColour == false)
                    setColour(DocumentWindow::textColourId, cabbgeEditor->fontColour); // <-- set customized titlebar font colour
                }
        }

       #if JUCE_IOS || JUCE_ANDROID
        auto screenBounds = Desktop::getInstance().getDisplays().getTotalBounds (true).toFloat();

        auto scaleFactor = jmin ((screenBounds.getWidth() - 50) / getWidth(), (screenBounds.getHeight() - 50) / getHeight());
        if (scaleFactor < 1.0f)
            setSize (getWidth() * scaleFactor, getHeight() * scaleFactor);

        setTopLeftPosition (20, 20);
       #else
        setTopLeftPosition (node->properties.getWithDefault (getLastXProp (type), Random::getSystemRandom().nextInt (500)),
                            node->properties.getWithDefault (getLastYProp (type), Random::getSystemRandom().nextInt (500)));
       #endif

        
        node->properties.set (getOpenProp (type), true);

        setVisible (true);
    }

    ~PluginWindow()
    {
        setLookAndFeel (nullptr);
        clearContentComponent();
    }


    void moved() override
    {
        node->properties.set (getLastXProp (type), getX());
        node->properties.set (getLastYProp (type), getY());
		//mod RW
		node->properties.set("PluginWindowX", getX());
		node->properties.set("PluginWindowY", getY());

    }

    void closeButtonPressed() override
    {
		//mod RW
		sendChangeMessage();
		node->getProcessor()->editorBeingDeleted(node->getProcessor()->getActiveEditor());
		

		node->properties.set (getOpenProp (type), false);
		setVisible(false);
		//activeWindowList.removeObject (this);
    }

    static String getLastXProp (Type type)    { return "uiLastX_" + getTypeName (type); }
    static String getLastYProp (Type type)    { return "uiLastY_" + getTypeName (type); }
    static String getOpenProp  (Type type)    { return "uiopen_"  + getTypeName (type); }

    OwnedArray<PluginWindow>& activeWindowList;
    const AudioProcessorGraph::Node::Ptr node;
    const Type type;

private:
    CabbageLookAndFeel2 pluginWindowLookAndFeel;

    float getDesktopScaleFactor() const override     { return 1.0f; }

    static AudioProcessorEditor* createProcessorEditor (AudioProcessor& processor, PluginWindow::Type type)
    {
        if (type == PluginWindow::Type::normal)
        {
            if (auto* ui = processor.createEditorIfNeeded())
            {

                return ui;
            }
            


            type = PluginWindow::Type::generic;
        }

        if (type == PluginWindow::Type::generic)
            return new GenericAudioProcessorEditor (&processor);

        if (type == PluginWindow::Type::programs)
            return new ProgramAudioProcessorEditor (processor);

        if (type == PluginWindow::Type::audioIO)
            return new FilterIOConfigurationWindow (processor);

        jassertfalse;
        return {};
    }

    static String getTypeName (Type type)
    {
        switch (type)
        {
            case Type::normal:     return "Normal";
            case Type::generic:    return "Generic";
            case Type::programs:   return "Programs";
            case Type::audioIO:    return "IO";
            default:               return {};
        }
    }

    //==============================================================================
    struct ProgramAudioProcessorEditor  : public AudioProcessorEditor
    {
        ProgramAudioProcessorEditor (AudioProcessor& p)  : AudioProcessorEditor (p)
        {
            setOpaque (true);

            addAndMakeVisible (panel);

            Array<PropertyComponent*> programs;

            auto numPrograms = p.getNumPrograms();
            int totalHeight = 0;

            for (int i = 0; i < numPrograms; ++i)
            {
                auto name = p.getProgramName (i).trim();

                if (name.isEmpty())
                    name = "Unnamed";

                auto pc = new PropertyComp (name, p);
                programs.add (pc);
                totalHeight += pc->getPreferredHeight();
            }

            panel.addProperties (programs);

            setSize (400, jlimit (25, 400, totalHeight));
        }

        void paint (Graphics& g) override
        {
            g.fillAll (Colours::grey);
        }

        void resized() override
        {
            panel.setBounds (getLocalBounds());
        }

    private:
        struct PropertyComp  : public PropertyComponent,
                               private AudioProcessorListener
        {
            PropertyComp (const String& name, AudioProcessor& p)  : PropertyComponent (name), owner (p)
            {
                owner.addListener (this);
            }

            ~PropertyComp()
            {
                owner.removeListener (this);
            }

            void refresh() override {}
            void audioProcessorChanged (AudioProcessor*) override {}
            void audioProcessorParameterChanged (AudioProcessor*, int, float) override {}

            AudioProcessor& owner;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertyComp)
        };

        PropertyPanel panel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
