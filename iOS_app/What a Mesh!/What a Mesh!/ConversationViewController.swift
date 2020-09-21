//
//  ConversationViewController.swift
//  What a Mesh!
//
//  Created by Gabriele Giuli on 2020-02-08.
//  Copyright © 2020 GabrieleGiuli. All rights reserved.
//

import UIKit
import MessengerKit
import Alamofire

class ConversationViewController: MSGMessengerViewController {
    
    let steve = User(displayName: "Steve", avatar: nil, avatarUrl: nil, isSender: true)
    
    let tim = User(displayName: "Tim", avatar: nil, avatarUrl: nil, isSender: false)
    
    var id = 100
    
    var address = "192.168.4.1"
    var this_user: ParsedUser?
    var recipient: ParsedUser?
    
    override var style: MSGMessengerStyle {
        var style = MessengerKit.Styles.iMessage
        style.headerHeight = 0
        return style
    }
    
    
    var messages: [[MSGMessage]] = []
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "iMessage"
        
        let name = UserDefaults.standard.string(forKey: "USER_FIRSTNAME")!
        let surname = UserDefaults.standard.string(forKey: "USER_LASTNAME")!
        let id = UserDefaults.standard.string(forKey: "USER_ID")!
        this_user = ParsedUser(name: name, ID: id)
        
        dataSource = self
        delegate = self
    }
    
    func addMessagesAtBeginning() {
        for message in self.recipient!.messages {
            id += 1
            print("Current Message: " + message)
            let body: MSGMessageBody = (message.containsOnlyEmoji && message.count < 5) ? .emoji(message) : .text(message)
            let message_b = MSGMessage(id: id, body: body, user: tim, sentAt: Date())
            
            self.messages.append([message_b])
        }
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        
        collectionView.scrollToBottom(animated: false)
    }
    
    override func inputViewPrimaryActionTriggered(inputView: MSGInputView) {
        id += 1
        
        let body: MSGMessageBody = (inputView.message.containsOnlyEmoji && inputView.message.count < 5) ? .emoji(inputView.message) : .text(inputView.message)
        
        let message = MSGMessage(id: id, body: body, user: steve, sentAt: Date())
        sendMessage(message: message)
        insert(message)
    }
    
    override func insert(_ message: MSGMessage) {
        
        collectionView.performBatchUpdates({
            if let lastSection = self.messages.last, let lastMessage = lastSection.last, lastMessage.user.displayName == message.user.displayName {
                self.messages[self.messages.count - 1].append(message)
                
                let sectionIndex = self.messages.count - 1
                let itemIndex = self.messages[sectionIndex].count - 1
                self.collectionView.insertItems(at: [IndexPath(item: itemIndex, section: sectionIndex)])
                
            } else {
                self.messages.append([message])
                let sectionIndex = self.messages.count - 1
                self.collectionView.insertSections([sectionIndex])
            }
        }, completion: { (_) in
            self.collectionView.scrollToBottom(animated: true)
            self.collectionView.layoutTypingLabelIfNeeded()
        })
        
    }
    
    func sendMessage(message: MSGMessage) {
        if let text = message.body.rawValue as? String {
            var requestString = "http://\(self.address)/message:" + processMessage(inString: text) + "/source_id:" + self.this_user!.ID + "/target_id:"
            requestString = requestString + self.recipient!.ID
            
            print("Request: " + requestString)
            
            Alamofire.request(requestString)
        }
    }
    
    func processMessage(inString: String) -> String {
        var newString = inString.replacingOccurrences(of: " ", with: "%20")
        newString = newString.replacingOccurrences(of: "\n", with: "%20")
        return newString
    }
    
    override func insert(_ messages: [MSGMessage], callback: (() -> Void)? = nil) {
        
        collectionView.performBatchUpdates({
            for message in messages {
                if let lastSection = self.messages.last, let lastMessage = lastSection.last, lastMessage.user.displayName == message.user.displayName {
                    self.messages[self.messages.count - 1].append(message)
                    
                    let sectionIndex = self.messages.count - 1
                    let itemIndex = self.messages[sectionIndex].count - 1
                    self.collectionView.insertItems(at: [IndexPath(item: itemIndex, section: sectionIndex)])
                    
                } else {
                    self.messages.append([message])
                    let sectionIndex = self.messages.count - 1
                    self.collectionView.insertSections([sectionIndex])
                }
            }
        }, completion: { (_) in
            self.collectionView.scrollToBottom(animated: false)
            self.collectionView.layoutTypingLabelIfNeeded()
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                callback?()
            }
        })
        
    }
    
}

// MARK: - Overrides

extension ConversationViewController {
    
}

// MARK: - MSGDataSource

extension ConversationViewController: MSGDataSource {
    
    func numberOfSections() -> Int {
        return messages.count
    }
    
    func numberOfMessages(in section: Int) -> Int {
        return messages[section].count
    }
    
    func message(for indexPath: IndexPath) -> MSGMessage {
        return messages[indexPath.section][indexPath.item]
    }
    
    func footerTitle(for section: Int) -> String? {
        return "Just now"
    }
    
    func headerTitle(for section: Int) -> String? {
        return messages[section].first?.user.displayName
    }
    
}

// MARK: - MSGDelegate

extension ConversationViewController: MSGDelegate {
    
    func linkTapped(url: URL) {
        print("Link tapped:", url)
    }
    
    func avatarTapped(for user: MSGUser) {
        print("Avatar tapped:", user)
    }
    
    func tapReceived(for message: MSGMessage) {
        print("Tapped: ", message)
    }
    
    func longPressReceieved(for message: MSGMessage) {
        print("Long press:", message)
    }
    
    func shouldDisplaySafari(for url: URL) -> Bool {
        return true
    }
    
    func shouldOpen(url: URL) -> Bool {
        return true
    }
    
}
