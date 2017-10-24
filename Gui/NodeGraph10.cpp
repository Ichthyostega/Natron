/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2013-2017 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "NodeGraph.h"
#include "NodeGraphPrivate.h"

#include <stdexcept>

GCC_DIAG_UNUSED_PRIVATE_FIELD_OFF
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QMouseEvent>
#include <QtCore/QString>
#include <QAction>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)
GCC_DIAG_UNUSED_PRIVATE_FIELD_ON

#include "Engine/Node.h"
#include "Engine/Project.h"
#include "Engine/CreateNodeArgs.h"

#include "Gui/BackdropGui.h"
#include "Gui/Edge.h"
#include "Gui/Gui.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/GuiApplicationManager.h"
#include "Gui/GuiMacros.h"
#include "Gui/NodeGui.h"
#include "Gui/NodeSettingsPanel.h"

#include "Global/QtCompat.h"


NATRON_NAMESPACE_ENTER


static NodeGuiPtr
isNodeGuiChild(QGraphicsItem* item)
{
    NodeGui* n = dynamic_cast<NodeGui*>(item);

    if (n) {
        return n->shared_from_this();
    }
    QGraphicsItem* parent = item->parentItem();
    if (parent) {
        return isNodeGuiChild(parent);
    } else {
        return NodeGuiPtr();
    }
}

static Edge*
isEdgeChild(QGraphicsItem* item)
{
    Edge* n = dynamic_cast<Edge*>(item);

    if (n) {
        return n;
    }
    QGraphicsItem* parent = item->parentItem();
    if (parent) {
        return isEdgeChild(parent);
    } else {
        return 0;
    }
}

void
NodeGraph::getNodesWithinViewportRect(const QRect& rect,
                                      std::set<NodeGuiPtr>* nodes) const
{
    QList<QGraphicsItem*> selectedItems = items(rect, Qt::IntersectsItemShape);
    for (QList<QGraphicsItem*>::Iterator it = selectedItems.begin(); it != selectedItems.end(); ++it) {
        NodeGuiPtr n = isNodeGuiChild(*it);
        if (n) {
            nodes->insert(n);
        }
    }
}

NodeGraph::NearbyItemEnum
NodeGraph::hasItemNearbyMouse(const QPoint& mousePosViewport,
                              NodeGuiPtr* node,
                              Edge** edge)
{
    assert(node && edge);
    *node = NodeGuiPtr();
    *edge = NULL;
    double tolerance = TO_DPIX(10.);
    QRect toleranceRect(mousePosViewport.x() - tolerance / 2.,
                        mousePosViewport.y() - tolerance / 2.,
                        tolerance,
                        tolerance);
    QList<QGraphicsItem*> selectedItems = items(toleranceRect, Qt::IntersectsItemShape);
    std::set<Edge*> edges;
    std::set<NodeGuiPtr> nodes;
    for (QList<QGraphicsItem*>::Iterator it = selectedItems.begin(); it != selectedItems.end(); ++it) {
        Edge* isEdge = isEdgeChild(*it);
        if (isEdge) {
            edges.insert(isEdge);
        }
        NodeGuiPtr n = isNodeGuiChild(*it);
        if (n) {
            nodes.insert(n);
        }
    }
    NodeGuiPtr nearbyBackdrop;
    NodeGraph::NearbyItemEnum nearbyBackdropType = NodeGraph::eNearbyItemNone;
    for (std::set<NodeGuiPtr>::iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if ( (*it)->isVisible() && (*it)->isActive() ) {
            QPointF localPoint = (*it)->mapFromScene( mapToScene(mousePosViewport) );
            BackdropGuiPtr isBd = toBackdropGui(*it);
            if (isBd) {
                if ( !nearbyBackdrop && isBd->isNearbyNameFrame(localPoint) ) {
                    nearbyBackdrop = *it;
                    nearbyBackdropType = eNearbyItemBackdropFrame;
                } else if ( !nearbyBackdrop && isBd->isNearbyResizeHandle(localPoint) ) {
                    nearbyBackdrop = *it;
                    nearbyBackdropType = eNearbyItemBackdropResizeHandle;
                }
            } else {
                *node = *it;

                return eNearbyItemNode;
            }
        }
    }
    if (nearbyBackdrop) {
        *node = nearbyBackdrop;
        return nearbyBackdropType;
    }
    if ( !edges.empty() ) {
        *edge = *edges.begin();

        QPointF scenePos = mapToScene(mousePosViewport);
        if ( (*edge)->hasSource() && (*edge)->isBendPointVisible() && (*edge)->isNearbyBendPoint(scenePos) ) {
            return eNearbyItemEdgeBendPoint;
        } else {
            return eNearbyItemNodeEdge;
        }

        return eNearbyItemNodeEdge;
    }

    return eNearbyItemNone;
} // NodeGraph::hasItemNearbyMouse

void
NodeGraph::mousePressEvent(QMouseEvent* e)
{
    assert(e);

    takeClickFocus();

    _imp->_hasMovedOnce = false;
    _imp->_deltaSinceMousePress = QPointF(0, 0);
    if ( buttonDownIsMiddle(e) ) {
        _imp->_evtState = eEventStateMovingArea;

        return;
    }

    bool didSomething = false;

    _imp->_lastMousePos = e->pos();
    const QPointF lastMousePosScene = mapToScene( _imp->_lastMousePos.x(), _imp->_lastMousePos.y() );

    if ( ( (e->buttons() & Qt::MiddleButton) &&
           ( ( buttonMetaAlt(e) == Qt::AltModifier) || (e->buttons() & Qt::LeftButton) ) ) ||
         ( (e->buttons() & Qt::LeftButton) &&
           ( buttonMetaAlt(e) == (Qt::AltModifier | Qt::MetaModifier) ) ) ) {
        // Alt + middle or Left + middle or Crtl + Alt + Left = zoom
        _imp->_evtState = eEventStateZoomingArea;

        return;
    }


    NodeCollectionPtr collection = getGroup();
    NodeGroupPtr isGroup = toNodeGroup(collection);
    bool isGroupEditable = true;
    bool groupEdited = true;
    if (isGroup) {
        isGroupEditable = isGroup->isSubGraphEditable();
        groupEdited = isGroup->isSubGraphEditedByUser();
    }


    if (!groupEdited) {
        if (isGroupEditable) {
            ///check if user clicked unlock
            int iw = _imp->unlockIcon.width();
            int ih = _imp->unlockIcon.height();
            int w = width();
            int offset = 20;
            if ( ( e->x() >= (w - iw - 10 - offset) ) && ( e->x() <= (w - 10 + offset) ) &&
                 ( e->y() >= (10 - offset) ) && ( e->y() <= (10 + ih + offset) ) ) {
                assert(isGroup);
                isGroup->setSubGraphEditedByUser(true);
                NodesList nodes = isGroup->getNodes();
                for (NodesList::iterator it = nodes.begin(); it != nodes.end(); ++it) {
                    NodeGuiPtr nodeUi = boost::dynamic_pointer_cast<NodeGui>( (*it)->getNodeGui() );
                    if (nodeUi) {
                        NodeSettingsPanel* panel = nodeUi->getSettingPanel();
                        if (panel) {
                            panel->setEnabled(true);
                        }
                    }
                }
            }
            getGui()->getApp()->triggerAutoSave();
            update();
        }
    }

    NodeGuiPtr nearbyNode;
    Edge* nearbyEdge;
    NearbyItemEnum nearbyItemCode = hasItemNearbyMouse(e->pos(), &nearbyNode, &nearbyEdge);
    if (nearbyItemCode == eNearbyItemBackdropResizeHandle) {
        assert(nearbyNode);
        didSomething = true;
        _imp->_backdropResized = nearbyNode->shared_from_this();
        _imp->_evtState = eEventStateResizingBackdrop;
    }


    if ( ( (nearbyItemCode == eNearbyItemNode) || (nearbyItemCode == eNearbyItemBackdropFrame) ) && groupEdited ) {
        assert(nearbyNode);
#pragma message WARN("TODO: if control is held, select all the nodes above, see https://github.com/MrKepzie/Natron/issues/497")
        NodeGuiPtr selectedNode = nearbyNode->shared_from_this();

        didSomething = true;
        if ( buttonDownIsLeft(e) ) {
            BackdropGuiPtr isBd = toBackdropGui( selectedNode );
            if (!isBd) {
                _imp->_magnifiedNode = selectedNode;
            }

            if ( !selectedNode->getIsSelected() ) {
                selectNode( selectedNode, modCASIsShift(e) );
            } else if ( modCASIsShift(e) ) {

                for (NodesGuiWList::iterator it = _imp->_selection.begin(); it != _imp->_selection.end(); ++it) {
                    NodeGuiPtr n = it->lock();
                    if ( n == selectedNode ) {

                        if (n) {
                            n->setUserSelected(false);
                        }
                        _imp->_selection.erase(it);
                        break;
                    }
                }
            }
            if ( groupEdited && (_imp->_evtState != eEventStateResizingBackdrop) ) {
                _imp->_evtState = eEventStateDraggingNode;
            }
            ///build the _nodesWithinBDAtPenDown map
            _imp->_nodesWithinBDAtPenDown.clear();
            for (NodesGuiWList::iterator it = _imp->_selection.begin(); it != _imp->_selection.end(); ++it) {
                BackdropGuiPtr isBd = toBackdropGui(it->lock());
                if (isBd) {
                    NodesGuiList nodesWithin = getNodesWithinBackdrop(isBd);
                    _imp->_nodesWithinBDAtPenDown.insert( std::make_pair(isBd, nodesWithin) );
                }
            }
        } else if ( buttonDownIsRight(e) ) {
            if ( !selectedNode->getIsSelected() ) {
                selectNode(selectedNode, true); //< don't wipe the selection
            }
        }
    }

    if ( (nearbyItemCode == eNearbyItemEdgeBendPoint) && groupEdited ) {
        _imp->setNodesBendPointsVisible(false);
        assert(nearbyEdge);
        ///Now connect the node to the edge input
        NodePtr inputNode = nearbyEdge->getSource()->getNode();
        assert(inputNode);
        ///disconnect previous connection
        NodePtr outputNode = nearbyEdge->getDest()->getNode();
        assert(outputNode);
        std::list<int> inputIndicesConnected = inputNode->getInputIndicesConnectedToThisNode(outputNode);
        if (inputIndicesConnected.empty()) {
            return;
        }

        CreateNodeArgsPtr args(CreateNodeArgs::create( PLUGINID_NATRON_DOT, _imp->group.lock() ));
        args->setProperty<bool>(kCreateNodeArgsPropAddUndoRedoCommand, false);
        args->setProperty<bool>(kCreateNodeArgsPropAutoConnect, false);
        args->setProperty<bool>(kCreateNodeArgsPropSettingsOpened, false);

        NodePtr dotNode = getGui()->getApp()->createNode(args);
        assert(dotNode);
        NodeGuiIPtr dotNodeGui_i = dotNode->getNodeGui();
        NodeGuiPtr dotNodeGui = boost::dynamic_pointer_cast<NodeGui>(dotNodeGui_i);
        assert(dotNodeGui);



        assert( getGui() );
        GuiAppInstancePtr guiApp = getGui()->getApp();
        assert(guiApp);
        ProjectPtr project = guiApp->getProject();
        assert(project);

        outputNode->disconnectInput(inputNode);
        dotNode->swapInput(inputNode, 0);

        for (std::list<int>::const_iterator it = inputIndicesConnected.begin(); it != inputIndicesConnected.end(); ++it) {
            outputNode->swapInput(dotNode, *it);
        }


        QPointF pos = dotNodeGui->mapToParent( dotNodeGui->mapFromScene(lastMousePosScene) );

        dotNodeGui->refreshPosition( pos.x(), pos.y() );
        if ( !dotNodeGui->getIsSelected() ) {
            selectNode( dotNodeGui, modCASIsShift(e) );
        }

        NodesList nodesList;
        nodesList.push_back(dotNode);

        pushUndoCommand( new AddMultipleNodesCommand( this, nodesList) );


        _imp->_evtState = eEventStateDraggingNode;
        didSomething = true;
    } else if ( (nearbyItemCode == eNearbyItemNodeEdge) && groupEdited ) {
        assert(nearbyEdge);
        _imp->_arrowSelected = nearbyEdge;
        didSomething = true;
        _imp->_evtState = eEventStateDraggingArrow;
    }

    ///Test if mouse is inside the navigator
    {
        QPointF mousePosSceneCoordinates;
        bool insideNavigator = isNearbyNavigator(e->pos(), mousePosSceneCoordinates);
        if (insideNavigator) {
            updateNavigator();
            _imp->_refreshOverlays = true;
            centerOn(mousePosSceneCoordinates);
            _imp->_evtState = eEventStateDraggingNavigator;
            didSomething = true;
        }
    }

    ///Don't forget to reset back to null the _backdropResized pointer
    if (_imp->_evtState != eEventStateResizingBackdrop) {
        _imp->_backdropResized.reset();
    }

    if (buttonDownIsRight(e) && groupEdited) {
        showMenu( mapToGlobal( e->pos() ) );
        didSomething = true;
    }
    if (!didSomething) {
        if ( buttonDownIsLeft(e) ) {
            if ( !modCASIsShift(e) ) {
                deselect();
            }
            _imp->_evtState = eEventStateSelectionRect;
            _imp->_lastSelectionStartPointScene = lastMousePosScene;
            _imp->_selectionRect = QRectF(lastMousePosScene.x(), lastMousePosScene.y(), 0, 0);
        } else if ( buttonDownIsMiddle(e) ) {
            _imp->_evtState = eEventStateMovingArea;
            QGraphicsView::mousePressEvent(e);
        }
    }
} // mousePressEvent

NATRON_NAMESPACE_EXIT
