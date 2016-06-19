//
//  NewDocumentViewController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class NewDocumentCollectionViewItem: NSCollectionViewItem {
	override var representedObject: AnyObject? {
		didSet {
			if let filename = representedObject as? String, imageView = imageView {
				imageView.image = NSImage(named: filename)
			}
		}
	}

	override var selected: Bool {
		didSet {
			var colour: NSColor!
			var lineColour: NSColor!
 
			if selected {
				colour		= NSColor.selectedControlColor()
				lineColour	= NSColor.blackColor()
			} else {
				colour		= NSColor.controlBackgroundColor()
				lineColour	= NSColor.controlBackgroundColor()
			}

			if let box = self.view as? NSBox {
				box.borderColor = lineColour
				box.fillColor = colour
			}
		}
	}
}

class NewDocumentViewController: NSViewController {

	@IBOutlet var collectionView : NSCollectionView!
	var collectionViewItems : [String] = []

    override func viewDidLoad() {
        super.viewDidLoad()

		collectionView.minItemSize = NSSize(width: 200, height: 150)

		let itemCopy: NewDocumentCollectionViewItem! = NewDocumentCollectionViewItem().copyWithZone(nil) as! NewDocumentCollectionViewItem
		collectionView.itemPrototype = itemCopy
		collectionView.content = ["ElectronBASIC", "Vic20BASIC"]
    }

	@IBAction func cancel(sender : AnyObject!) {
		self.view.window?.close()
	}

	@IBAction func choose(sender : AnyObject!) {
		print("\(collectionView.selectionIndexes)")
	}
}
